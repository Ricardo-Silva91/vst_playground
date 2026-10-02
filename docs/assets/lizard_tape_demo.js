// Live demo for docs/lizard_tape.html. Same structure as lizard_suite_demo.js.
// The DSP itself is in lizard_tape_dsp.js (a JS port of lizard_tape/Source/DSP/TapeProcessor.h).
(function () {
const DSP_SRC = window.LIZARD_TAPE_DSP;
const M = new Function(DSP_SRC + "\nreturn { LizardTape };")();

// ---------- parameters (ranges, defaults and skews match createLayout) ----------
const skewed = (s, e, k) => ({
  toValue: p => p <= 0 ? s : s + (e - s) * Math.exp(Math.log(p) / k),
  toProp: v => v <= s ? 0 : Math.min(1, Math.exp(Math.log((v - s) / (e - s)) * k)),
});
const lin = (s, e) => skewed(s, e, 1);
const snap = (v, step) => Math.round(v / step) * step;
const fmtHz = f => f >= 1000 ? (f / 1000).toFixed(f >= 10000 ? 1 : 2).replace(/\.?0+$/, "") + " kHz" : Math.round(f) + " Hz";
const pct = v => Math.round(v * 100) + " %";
const sdb = v => (v > 0 ? "+" : v < 0 ? "−" : "") + Math.abs(v).toFixed(1) + " dB";
let hostRate = 48000;

const MODULES = [
  { key: "sat", name: "Saturation", role: "drive · bias", params: [
    { id: "drive", label: "Drive", def: 4, map: lin(0, 24), step: 0.01, fmt: v => "+" + v.toFixed(1) + " dB" },
    { id: "bias", label: "Sat Bias", def: 0.15, map: lin(0, 1), step: 0.001, fmt: v => v.toFixed(3) },
  ] },
  { key: "wf", name: "Wow & Flutter", role: "transport", off: () => P.wowDepth <= 0 && P.flutterDepth <= 0, params: [
    { id: "wowDepth", label: "Wow Depth", def: 3, map: lin(0, 10), step: 0.01, fmt: v => v.toFixed(2) + " ms" },
    { id: "wowRate", label: "Wow Rate", def: 0.7, map: skewed(0.05, 4, 0.5), step: 0.001, fmt: v => v.toFixed(2) + " Hz" },
    { id: "flutterDepth", label: "Flutter Depth", def: 0.6, map: lin(0, 3), step: 0.001, fmt: v => v.toFixed(2) + " ms" },
    { id: "flutterRate", label: "Flutter Rate", def: 8, map: lin(2, 16), step: 0.01, fmt: v => v.toFixed(1) + " Hz" },
  ] },
  { key: "tape", name: "Tone & Hiss", role: "tape", params: [
    { id: "tone", label: "Tone", def: 6500, map: skewed(800, 18000, 0.35), step: 1, fmt: fmtHz },
    { id: "hiss", label: "Hiss", def: 0.06, map: lin(0, 1), step: 0.001, fmt: pct },
  ] },
  { key: "out", name: "Mix & Output", role: "out", off: () => P.mix <= 0, offText: "DRY", params: [
    { id: "mix", label: "Mix", def: 1, map: lin(0, 1), step: 0.001, fmt: pct },
    { id: "output", label: "Output", def: 0, map: lin(-24, 12), step: 0.01, fmt: sdb },
  ] },
];
const PDEF = {};
MODULES.forEach(m => m.params.forEach(p => { PDEF[p.id] = p; }));
const DEFAULTS = Object.fromEntries(Object.values(PDEF).map(p => [p.id, p.def]));
const P = { ...DEFAULTS };

const PRESETS = [
  { name: "Plugin defaults", p: { ...DEFAULTS } },
  { name: "Worn Walkman", p: { drive: 8, bias: 0.3, wowDepth: 5, wowRate: 0.9, flutterDepth: 1.2, flutterRate: 9, tone: 4500, hiss: 0.25 } },
  { name: "Warm deck", p: { drive: 10, bias: 0.4, wowDepth: 0.8, wowRate: 0.5, flutterDepth: 0.2, flutterRate: 8, tone: 12000, hiss: 0.03 } },
  { name: "Seasick", p: { drive: 4, wowDepth: 10, wowRate: 0.35, flutterDepth: 0.4, flutterRate: 6, tone: 7000, hiss: 0.1 } },
  { name: "Dictaphone", p: { drive: 14, bias: 0.2, wowDepth: 2, wowRate: 1.5, flutterDepth: 2.2, flutterRate: 12, tone: 2200, hiss: 0.5 } },
  { name: "Clean", p: { mix: 0 } },
];

const $ = id => document.getElementById(id);
let actx = null;
try { actx = new (window.AudioContext || window.webkitAudioContext)(); hostRate = actx.sampleRate; } catch (e) { actx = null; }

// ---------- rack UI ----------
MODULES.forEach((m, i) => {
  const el = document.createElement("div"); el.className = "lz-module"; el.setAttribute("role", "group"); el.setAttribute("aria-label", m.name);
  let h = '<div class="lz-mod-head"><span class="lz-mod-num">0' + (i + 1) + '</span><h3 class="lz-mod-name">' + m.name + '</h3><span class="lz-mod-role">' + m.role + '</span>' +
    (m.off ? '<span class="lz-mod-state" id="' + m.key + 'State">ON</span>' : '') + '</div>';
  for (const p of m.params) {
    h += '<div class="lz-ctl"><label for="' + p.id + '">' + p.label + '</label><output class="lz-lcd" id="' + p.id + 'Out" for="' + p.id + '"></output>' +
      '<input id="' + p.id + '" type="range" min="0" max="10000" step="1"></div>';
  }
  h += '<dl class="lz-stats" id="' + m.key + 'Stats"></dl>';
  el.innerHTML = h;
  $("rack").appendChild(el);
});
Object.values(PDEF).forEach(p => {
  $(p.id).addEventListener("input", e => { P[p.id] = snap(p.map.toValue(+e.target.value / 10000), p.step); changed(); });
});

function stat(dt, dd) { return '<div class="lz-stat"><dt>' + dt + '</dt><dd>' + dd + '</dd></div>'; }
const dB = v => v <= 0 ? "−∞ dB" : (20 * Math.log10(v)).toFixed(0).replace("-", "−") + " dBFS";
const toDb = v => 20 * Math.log10(v);
// Hiss RMS per unit Hiss: 0.05 * rms(0.4*lp + 0.3*white) for the one-pole
// (a = 0.05) brown part and uniform white noise, times the program factor.
const HISS_RMS = 0.05 * Math.sqrt(0.16 * (0.0025 / 0.0975) / 3 + 0.09 / 3 + 0.24 * 0.05 / 3);
function derived() {
  const sr = hostRate, s = {};
  const g = Math.pow(10, P.drive / 20), makeup = 1 / Math.max(0.25, g * 0.7), hb = P.bias * 0.5;
  const sech2 = 1 - Math.tanh(hb) ** 2;
  s.sat = stat("Into the curve", "×" + g.toFixed(2)) + stat("Makeup", sdb(toDb(makeup))) +
    stat("Small-signal gain", sdb(toDb(g * sech2 * makeup))) + stat("Knee at input", dB(Math.min(1, 1 / g)));
  const wowDev = 0.5 * P.wowDepth * 1e-3 * 2 * Math.PI * P.wowRate, flDev = P.flutterDepth * 1e-3 * 2 * Math.PI * P.flutterRate;
  const cents = d => "±" + (1200 * Math.log2(1 + d)).toFixed(1) + " ct";
  s.wf = stat("Wow pitch swing", cents(wowDev)) + stat("Flutter swing", cents(flDev)) +
    stat("Delay range", (12 - 0.5 * P.wowDepth - P.flutterDepth).toFixed(1) + "–" + (12 + 0.5 * P.wowDepth + P.flutterDepth).toFixed(1) + " ms") +
    stat("Wet delay", "12 ms, unreported");
  const fc = Math.min(Math.max(P.tone, 500), sr * 0.45), hissOut = P.hiss * HISS_RMS * P.mix * Math.pow(10, P.output / 20);
  s.tape = stat("Roll-off −3 dB", "≈ " + fmtHz(fc)) + stat("At 15 kHz", fc < 15000 ? sdb(-10 * Math.log10(1 + (15000 / fc) ** 2)) : "≈ flat") +
    stat("Hiss on silence", dB(hissOut * 0.625).replace("dBFS", "dB RMS")) + stat("Hiss under signal", dB(hissOut).replace("dBFS", "dB RMS"));
  const partial = P.mix > 0 && P.mix < 1;
  s.out = stat("Dry / wet", Math.round((1 - P.mix) * 100) + " / " + Math.round(P.mix * 100)) + stat("Output gain", "×" + Math.pow(10, P.output / 20).toFixed(2)) +
    stat("Comb notches", partial ? "every 83 Hz" : "none") + stat("First notch", partial ? "≈ 42 Hz" : "—");
  return s;
}

function syncUI() {
  for (const p of Object.values(PDEF)) {
    $(p.id).value = Math.round(p.map.toProp(P[p.id]) * 10000);
    const t = p.fmt(P[p.id]); $(p.id + "Out").textContent = t; $(p.id).setAttribute("aria-valuetext", t);
  }
  const d = derived();
  for (const m of MODULES) {
    $(m.key + "Stats").innerHTML = d[m.key];
    if (!m.off) continue;
    const off = m.off(), st = $(m.key + "State");
    st.textContent = off ? (m.offText || "OFF") : "ON"; st.classList.toggle("off", off);
  }
  document.querySelectorAll("#lzPresets button").forEach((b, i) => {
    const pp = { ...DEFAULTS, ...PRESETS[i].p };
    b.setAttribute("aria-pressed", String(Object.keys(P).every(k => Math.abs(pp[k] - P[k]) < 1e-6)));
  });
}
function changed() { syncUI(); sendParams(); queueStatic(); }
PRESETS.forEach(pr => {
  const b = document.createElement("button"); b.textContent = pr.name; b.setAttribute("aria-pressed", "false");
  b.addEventListener("click", () => { Object.assign(P, DEFAULTS, pr.p); changed(); });
  $("lzPresets").appendChild(b);
});

// ---------- test sources ----------
function makeLoop(sr) {
  const step = 60 / 90 / 4, len = Math.round(32 * step * sr);
  const L = new Float32Array(len), R = new Float32Array(len);
  let seed = 7;
  const rnd = () => { seed = (seed * 1664525 + 1013904223) >>> 0; return seed / 2147483648 - 1; };
  const put = (i, l, r) => { if (i < len) { L[i] += l; R[i] += r; } };
  const kick = t0 => { const s = Math.round(t0 * sr), n = Math.round(0.5 * sr); let ph = 0;
    for (let i = 0; i < n; i++) { const t = i / sr; ph += 2 * Math.PI * (44 + 85 * Math.exp(-t / 0.04)) / sr; const v = Math.sin(ph) * Math.exp(-t / 0.26) * 0.95; put(s + i, v, v); } };
  const snare = t0 => { const s = Math.round(t0 * sr), n = Math.round(0.32 * sr); let y = 0;
    for (let i = 0; i < n; i++) { const t = i / sr; y += 0.55 * (rnd() - y); const v = y * Math.exp(-t / 0.11) * 0.6 + Math.sin(2 * Math.PI * 188 * t) * Math.exp(-t / 0.045) * 0.4; put(s + i, v, v); } };
  const hat = (t0, open, vel, pan) => { const s = Math.round(t0 * sr), n = Math.round((open ? 0.4 : 0.07) * sr); let prev = 0, prev2 = 0;
    for (let i = 0; i < n; i++) { const t = i / sr, x = rnd(), h = x - 2 * prev + prev2; prev2 = prev; prev = x; const v = h * Math.exp(-t / (open ? 0.13 : 0.02)) * 0.09 * vel; put(s + i, v * (1 - pan), v * (1 + pan)); } };
  const keys = (t0, notes, amp) => { const s = Math.round(t0 * sr), n = Math.round(2.6 * sr);
    for (const m of notes) { const f = 440 * Math.pow(2, (m - 69) / 12);
      for (let i = 0; i < n; i++) { const t = i / sr, att = Math.min(1, t / 0.004);
        const v = att * amp * (Math.sin(2 * Math.PI * f * t) * Math.exp(-t / 1.5) + 0.3 * Math.sin(2 * Math.PI * 2 * f * t) * Math.exp(-t / 0.5) + 0.14 * Math.sin(2 * Math.PI * 7.02 * f * t) * Math.exp(-t / 0.07));
        put(s + i, v, v); } } };
  const at = k => k * step + (k % 2 ? step * 0.17 : 0);
  [0, 7, 10, 16, 22, 26, 29].forEach(k => kick(at(k)));
  [4, 12, 20, 28].forEach(k => snare(at(k)));
  for (let k = 0; k < 32; k += 2) if (k !== 30) hat(at(k), false, k % 4 ? 0.7 : 1, 0.25);
  hat(at(30), true, 0.9, 0.25);
  keys(at(0), [50, 57, 60, 64, 65], 0.085); keys(at(10), [60, 64, 65], 0.05);
  keys(at(16), [46, 57, 60, 62, 65], 0.085); keys(at(26), [57, 62, 65], 0.05);
  let peak = 0; for (let i = 0; i < len; i++) peak = Math.max(peak, Math.abs(L[i]), Math.abs(R[i]));
  const g = 0.85 / peak; for (let i = 0; i < len; i++) { L[i] *= g; R[i] *= g; }
  return { chans: [L, R], sr, previewAt: Math.round(at(4) * sr) };
}
function makeSweep(sr) {
  const T = 8, len = Math.round(T * sr), X = new Float32Array(len); let ph = 0;
  for (let i = 0; i < len; i++) { const t = i / sr; ph += 2 * Math.PI * 20 * Math.pow(1000, t / T) / sr; X[i] = 0.6 * Math.sin(ph) * Math.min(1, t / 0.02, (T - t) / 0.02); }
  return { chans: [X, X], sr, previewAt: Math.round(T * Math.log(350) / Math.log(1000) * sr) };
}
function makeTone(sr) {
  const len = Math.round(4 * sr), X = new Float32Array(len);
  for (let i = 0; i < len; i++) X[i] = 0.5 * Math.sin(2 * Math.PI * 1000 * i / sr) * Math.min(1, i / (0.01 * sr));
  return { chans: [X, X], sr, previewAt: Math.round(sr) };
}
function makeSilence(sr) { const X = new Float32Array(Math.round(4 * sr)); return { chans: [X, X], sr, previewAt: Math.round(sr) }; }
const sources = { loop: makeLoop(hostRate), tone: makeTone(hostRate), sweep: makeSweep(hostRate), silence: makeSilence(hostRate), file: null };
let current = "loop";
function toAudioBuffer(src) { const b = actx.createBuffer(2, src.chans[0].length, src.sr); b.copyToChannel(src.chans[0], 0); b.copyToChannel(src.chans[1], 1); return b; }

// ---------- FFT ----------
const N = 8192;
const fft = (() => {
  const rev = new Uint32Array(N), bits = Math.log2(N);
  for (let i = 0; i < N; i++) { let r = 0; for (let b = 0; b < bits; b++) r |= ((i >> b) & 1) << (bits - 1 - b); rev[i] = r; }
  const cs = new Float64Array(N / 2), sn = new Float64Array(N / 2);
  for (let i = 0; i < N / 2; i++) { cs[i] = Math.cos(2 * Math.PI * i / N); sn[i] = Math.sin(2 * Math.PI * i / N); }
  return (re, im) => {
    for (let i = 0; i < N; i++) { const j = rev[i]; if (j > i) { let t = re[i]; re[i] = re[j]; re[j] = t; t = im[i]; im[i] = im[j]; im[j] = t; } }
    for (let size = 2; size <= N; size <<= 1) { const half = size >> 1, st = N / size;
      for (let i = 0; i < N; i += size) for (let j = 0; j < half; j++) { const k = j * st, a = i + j, b = a + half;
        const tr = re[b] * cs[k] + im[b] * sn[k], ti = im[b] * cs[k] - re[b] * sn[k];
        re[b] = re[a] - tr; im[b] = im[a] - ti; re[a] += tr; im[a] += ti; } }
  };
})();
const hann = new Float64Array(N); let hannSum = 0;
for (let i = 0; i < N; i++) { hann[i] = 0.5 - 0.5 * Math.cos(2 * Math.PI * i / (N - 1)); hannSum += hann[i]; }
const re = new Float64Array(N), im = new Float64Array(N);
function power(x, out) {
  for (let i = 0; i < N; i++) { re[i] = x[i] * hann[i]; im[i] = 0; }
  fft(re, im);
  const s = 2 / hannSum;
  for (let k = 0; k < N / 2; k++) { const m = Math.hypot(re[k], im[k]) * s; out[k] = m * m; }
}

// ---------- drawing ----------
const css = n => getComputedStyle(document.documentElement).getPropertyValue(n).trim();
function fit(c) {
  const r = c.getBoundingClientRect(), d = window.devicePixelRatio || 1;
  const w = Math.max(1, Math.round(r.width * d)), h = Math.max(1, Math.round(r.height * d));
  if (c.width !== w || c.height !== h) { c.width = w; c.height = h; }
  const g = c.getContext("2d"); g.setTransform(d, 0, 0, d, 0, 0);
  return { g, W: r.width, H: r.height };
}
function marker(g, x, label, dash, color, y, pl, pw, pt, ph) {
  g.save(); g.setLineDash(dash); g.strokeStyle = color; g.lineWidth = 1.5;
  g.beginPath(); g.moveTo(Math.round(x) + 0.5, pt); g.lineTo(Math.round(x) + 0.5, pt + ph); g.stroke(); g.restore();
  g.fillStyle = color; g.font = "600 11px " + css("--mono");
  const right = x + 6 + g.measureText(label).width < pl + pw;
  g.textAlign = right ? "left" : "right"; g.fillText(label, right ? x + 6 : x - 6, y);
}
function drawSpectrum(pDry, pWet, sr) {
  const { g, W, H } = fit($("spec"));
  const pl = 38, pr = 10, pt = 12, pb = 22, pw = W - pl - pr, ph = H - pt - pb;
  const fmin = 20, fmax = sr / 2, lr = Math.log(fmax / fmin);
  const xOf = f => pl + Math.log(f / fmin) / lr * pw;
  const yOf = db => pt + Math.min(1, Math.max(0, -db / 120)) * ph;
  g.clearRect(0, 0, W, H); g.font = "11px " + css("--mono"); g.textBaseline = "middle";
  const wet = P.mix > 0, fc = Math.min(Math.max(P.tone, 500), sr * 0.45);
  if (wet && fc < fmax) { g.fillStyle = css("--shade"); g.fillRect(xOf(fc), pt, pl + pw - xOf(fc), ph); }
  g.strokeStyle = css("--grid"); g.fillStyle = css("--muted"); g.lineWidth = 1;
  for (let db = -20; db >= -120; db -= 20) { const y = Math.round(yOf(db)) + 0.5; g.beginPath(); g.moveTo(pl, y); g.lineTo(pl + pw, y); g.stroke(); g.textAlign = "right"; g.fillText(db, pl - 6, y); }
  g.textAlign = "center";
  for (const f of [50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000]) {
    if (f > fmax) continue;
    const x = Math.round(xOf(f)) + 0.5; g.beginPath(); g.moveTo(x, pt); g.lineTo(x, pt + ph); g.stroke();
    if ([100, 1000, 10000].includes(f) || (f === 20000 && pw > 380)) g.fillText(fmtHz(f).replace(" ", ""), x, H - 10);
  }
  const binHz = sr / N;
  const column = (p, px) => { const f0 = fmin * Math.exp(px / pw * lr), f1 = fmin * Math.exp((px + 1) / pw * lr);
    const k0 = Math.max(1, Math.floor(f0 / binHz)), k1 = Math.min(N / 2 - 1, Math.max(k0, Math.floor(f1 / binHz)));
    let m = 0; for (let k = k0; k <= k1; k++) m = Math.max(m, p[k]); return 10 * Math.log10(m + 1e-14); };
  const path = p => { const pts = []; for (let px = 0; px <= pw; px++) pts.push([pl + px, yOf(column(p, px))]); return pts; };
  const dry = path(pDry), wt = path(pWet);
  g.beginPath(); g.moveTo(pl, pt + ph); wt.forEach(([x, y]) => g.lineTo(x, y)); g.lineTo(pl + pw, pt + ph); g.closePath();
  g.globalAlpha = 0.22; g.fillStyle = css("--accent"); g.fill(); g.globalAlpha = 1;
  g.lineWidth = 1.25; g.strokeStyle = css("--dry"); g.beginPath(); dry.forEach(([x, y], i) => i ? g.lineTo(x, y) : g.moveTo(x, y)); g.stroke();
  g.lineWidth = 1.5; g.strokeStyle = css("--accent"); g.beginPath(); wt.forEach(([x, y], i) => i ? g.lineTo(x, y) : g.moveTo(x, y)); g.stroke();
  if (wet && fc < fmax) marker(g, xOf(fc), "Tone " + fmtHz(fc), [5, 4], css("--marker"), pt + 10, pl, pw, pt, ph);
}
const SCOPE_N = 160;
function drawScope(dry, wet, off) {
  const { g, W, H } = fit($("scope"));
  const pl = 8, pr = 8, pt = 10, pb = 10, pw = W - pl - pr, ph = H - pt - pb;
  let peak = 0.02; for (let i = 0; i < SCOPE_N; i++) peak = Math.max(peak, Math.abs(dry[off + i]), Math.abs(wet[off + i]));
  const sc = 1 / Math.min(1.5, peak * 1.1);
  const xOf = i => pl + i / SCOPE_N * pw, yOf = v => pt + ph / 2 - v * sc * ph / 2;
  g.clearRect(0, 0, W, H);
  g.strokeStyle = css("--grid"); g.lineWidth = 1;
  g.beginPath(); g.moveTo(pl, Math.round(pt + ph / 2) + 0.5); g.lineTo(pl + pw, Math.round(pt + ph / 2) + 0.5); g.stroke();
  g.lineWidth = 1.5; g.strokeStyle = css("--dry");
  g.beginPath(); for (let i = 0; i < SCOPE_N; i++) { const x = xOf(i + 0.5), y = yOf(dry[off + i]); i ? g.lineTo(x, y) : g.moveTo(x, y); } g.stroke();
  g.lineWidth = 1.75; g.strokeStyle = css("--accent"); g.lineJoin = "miter";
  g.beginPath(); for (let i = 0; i < SCOPE_N; i++) { const y = yOf(wet[off + i]); if (i === 0) g.moveTo(xOf(0), y); else g.lineTo(xOf(i), y); g.lineTo(xOf(i + 1), y); } g.stroke();
  g.fillStyle = css("--muted"); g.font = "11px " + css("--mono"); g.textAlign = "right"; g.textBaseline = "top";
  g.fillText("±" + (1 / sc).toFixed(2) + " FS", W - 10, 6);
}
function drawPitch(cents, secs) {
  const { g, W, H } = fit($("pitch"));
  const pl = 44, pr = 10, pt = 10, pb = 22, pw = W - pl - pr, ph = H - pt - pb;
  let span = 10; for (const c of cents) if (isFinite(c)) span = Math.max(span, Math.abs(c));
  span = [10, 25, 50, 100, 200, 400].find(s => s >= span * 1.05) || 400;
  const yOf = c => pt + ph / 2 - Math.max(-1, Math.min(1, c / span)) * ph / 2;
  g.clearRect(0, 0, W, H); g.font = "11px " + css("--mono"); g.textBaseline = "middle";
  g.strokeStyle = css("--grid"); g.fillStyle = css("--muted"); g.lineWidth = 1;
  for (const c of [-span, -span / 2, 0, span / 2, span]) { const y = Math.round(yOf(c)) + 0.5; g.beginPath(); g.moveTo(pl, y); g.lineTo(pl + pw, y); g.stroke(); g.textAlign = "right"; g.fillText((c > 0 ? "+" : c < 0 ? "−" : "") + Math.abs(c) + "ct", pl - 6, y); }
  g.textAlign = "center";
  for (let s = 0; s <= secs; s++) { const x = Math.round(pl + s / secs * pw) + 0.5; g.beginPath(); g.moveTo(x, pt); g.lineTo(x, pt + ph); g.stroke(); g.fillText(s + " s", Math.min(W - 16, Math.max(pl + 8, x)), H - 10); }
  g.lineWidth = 1.25; g.strokeStyle = css("--dry"); g.beginPath(); g.moveTo(pl, Math.round(yOf(0)) + 0.5); g.lineTo(pl + pw, Math.round(yOf(0)) + 0.5); g.stroke();
  g.lineWidth = 1.5; g.strokeStyle = css("--accent"); g.beginPath();
  let pen = false;
  cents.forEach((c, i) => { if (!isFinite(c)) { pen = false; return; } const x = pl + (i + 0.5) / cents.length * pw, y = yOf(c); pen ? g.lineTo(x, y) : g.moveTo(x, y); pen = true; });
  g.stroke();
}

// ---------- static preview (the same port, run offline) ----------
const pD = new Float64Array(N / 2), pW = new Float64Array(N / 2), PITCH_S = 4;
function runCore(x, sr) {
  const y = Float32Array.from(x), t = new M.LizardTape(1); t.prepareToPlay(sr, P);
  for (let o = 0; o < y.length; o += 128) t.processBlock([y.subarray(o, Math.min(y.length, o + 128))], Math.min(128, y.length - o), P);
  return y;
}
// Pitch of the 1 kHz tone after the core, from zero-crossing intervals in 10 ms windows.
function pitchTrack(sr) {
  const tone = sources.tone.chans[0], y = bypass ? tone : runCore(tone, sr);
  const win = Math.round(0.01 * sr), bins = Math.floor(y.length / win), sum = new Float64Array(bins), cnt = new Uint32Array(bins);
  let last = -1;
  for (let i = Math.round(0.03 * sr); i < y.length; i++) {
    if (y[i - 1] < 0 && y[i] >= 0) {
      const t = i - 1 + y[i - 1] / (y[i - 1] - y[i]);
      if (last >= 0) { const b = Math.floor(t / win); if (b < bins) { sum[b] += t - last; cnt[b]++; } }
      last = t;
    }
  }
  const cents = new Array(bins);
  for (let b = 0; b < bins; b++) cents[b] = cnt[b] ? 1200 * Math.log2(sr / (sum[b] / cnt[b]) / 1000) : NaN;
  return cents;
}
function renderStatic() {
  const src = sources[current] || sources.loop, sr = src.sr;
  const len = Math.min(src.chans[0].length, src.previewAt + N + 2048);
  const start = Math.max(0, Math.min(src.previewAt, len - N));
  const dry = src.chans[0].subarray(0, len), wet = bypass ? dry : runCore(dry, sr);
  power(dry.subarray(start, start + N), pD); power(wet.subarray(start, start + N), pW);
  drawSpectrum(pD, pW, sr);
  drawScope(dry, wet, start + 1024);
  renderPitch();
}
function renderPitch() { drawPitch(pitchTrack(hostRate), PITCH_S); }
let staticQueued = false;
function queueStatic() {
  if (staticQueued) return;
  staticQueued = true;
  // While playing, the live analysers own the spectrum and scope; the pitch
  // plot still follows the controls from an offline render.
  requestAnimationFrame(() => { staticQueued = false; if (playing) renderPitch(); else renderStatic(); });
}

document.querySelectorAll("#lizardDemo [data-try]").forEach(b => b.addEventListener("click", () => {
  const t = b.dataset.try;
  if (t === "dry") { Object.assign(P, { drive: 24, bias: 1, wowDepth: 10, wowRate: 4, flutterDepth: 3, flutterRate: 16, tone: 800, hiss: 1, mix: 0, output: 0 }); }
  if (t === "drive") { Object.assign(P, DEFAULTS, { drive: 24, bias: 1, wowDepth: 0, flutterDepth: 0, tone: 18000, hiss: 0 }); selectSource("sweep", true); }
  if (t === "hiss") { Object.assign(P, DEFAULTS, { hiss: 0.6 }); selectSource("silence", true); }
  if (t === "wobble") { Object.assign(P, DEFAULTS, { wowDepth: 8, wowRate: 0.7, flutterDepth: 1.5, flutterRate: 8, hiss: 0 }); selectSource("tone", true); }
  if (t === "comb") { Object.assign(P, DEFAULTS, { wowDepth: 0, flutterDepth: 0, drive: 0, bias: 0, tone: 18000, hiss: 0, mix: 0.5 }); selectSource("sweep", true); }
  changed();
  start(); // restarts with the card's source if one is already playing
}));

// ---------- audio graph ----------
let node = null, sendParams = () => {}, pre = null, post = null, srcNode = null, playing = false, bypass = false, graphReady = null;
const WORKLET = DSP_SRC + "\n" + String.raw`
class LTProc extends AudioWorkletProcessor {
  constructor() {
    super(); this.t = new LizardTape(2); this.p = null; this.bypass = false;
    this.port.onmessage = e => { const m = e.data;
      if (m.type === "params") this.p = m.p;
      else if (m.type === "bypass") this.bypass = m.on;
      else if (m.type === "reset") { this.p = m.p; this.t.prepareToPlay(sampleRate, m.p); } };
  }
  process(inputs, outputs) {
    const inp = inputs[0], out = outputs[0];
    for (let ch = 0; ch < out.length; ch++) {
      if (!inp || inp.length === 0) out[ch].fill(0); else out[ch].set(inp[Math.min(ch, inp.length - 1)]);
    }
    if (!this.bypass && this.p) this.t.processBlock(out, out[0].length, this.p);
    return true;
  }
}
registerProcessor("lizard-tape", LTProc);`;

async function buildGraph() {
  pre = actx.createAnalyser(); pre.fftSize = N; pre.smoothingTimeConstant = 0;
  post = actx.createAnalyser(); post.fftSize = N; post.smoothingTimeConstant = 0;
  const out = actx.createGain(); out.gain.value = 0.9;
  try {
    const url = URL.createObjectURL(new Blob([WORKLET], { type: "application/javascript" }));
    await actx.audioWorklet.addModule(url);
    node = new AudioWorkletNode(actx, "lizard-tape", { numberOfInputs: 1, numberOfOutputs: 1, outputChannelCount: [2] });
    sendParams = () => node.port.postMessage({ type: "params", p: { ...P } });
    node.setBypass = on => node.port.postMessage({ type: "bypass", on });
    node.resetDsp = () => node.port.postMessage({ type: "reset", p: { ...P } });
  } catch (e) {
    node = actx.createScriptProcessor(1024, 2, 2);
    const tape = new M.LizardTape(2); let by = false;
    node.onaudioprocess = ev => {
      const o = [0, 1].map(ch => { const b = ev.outputBuffer.getChannelData(ch); b.set(ev.inputBuffer.getChannelData(Math.min(ch, ev.inputBuffer.numberOfChannels - 1))); return b; });
      if (by) return;
      for (let k = 0; k < o[0].length; k += 128) tape.processBlock(o.map(b => b.subarray(k, k + 128)), Math.min(128, o[0].length - k), P);
    };
    sendParams = () => {};
    node.setBypass = on => { by = on; };
    node.resetDsp = () => tape.prepareToPlay(actx.sampleRate, P);
  }
  node.connect(post); post.connect(out); out.connect(actx.destination);
  node.setBypass(bypass);
}
async function start() {
  if (!actx) { $("status").textContent = "This browser has no Web Audio support, so only the static preview works."; return; }
  try { await actx.resume(); if (!graphReady) graphReady = buildGraph(); await graphReady; }
  catch (e) { $("status").textContent = "Couldn't start audio: " + e.message; return; }
  stopSource();
  const src = sources[current];
  srcNode = actx.createBufferSource(); srcNode.buffer = src.buffer || (src.buffer = toAudioBuffer(src)); srcNode.loop = true;
  srcNode.connect(pre); srcNode.connect(node);
  node.resetDsp(); sendParams(); srcNode.start();
  playing = true; $("playBtn").textContent = "Stop";
  $("status").textContent = "Playing " + label(current) + (bypass ? " (bypassed)" : "") + ".";
  loop();
}
function stopSource() { if (srcNode) { try { srcNode.stop(); } catch (e) {} srcNode.disconnect(); srcNode = null; } }
function stop() { stopSource(); playing = false; $("playBtn").textContent = "Play"; $("status").textContent = "Stopped. The plots are a static preview."; queueStatic(); }
const label = k => ({ loop: "the drum loop", tone: "the 1 kHz tone", sweep: "the 20 Hz – 20 kHz sweep", silence: "silence" })[k] || (sources.file && sources.file.name) || "your file";

const tdPre = new Float32Array(N), tdPost = new Float32Array(N), lpD = new Float64Array(N / 2), lpW = new Float64Array(N / 2);
const smD = new Float64Array(N / 2), smW = new Float64Array(N / 2);
function loop() {
  if (!playing) return;
  pre.getFloatTimeDomainData(tdPre); post.getFloatTimeDomainData(tdPost);
  power(tdPre, lpD); power(tdPost, lpW);
  for (let k = 0; k < N / 2; k++) { smD[k] = smD[k] * 0.55 + lpD[k] * 0.45; smW[k] = smW[k] * 0.55 + lpW[k] * 0.45; }
  drawSpectrum(smD, smW, actx.sampleRate);
  drawScope(tdPre, tdPost, N - SCOPE_N);
  requestAnimationFrame(loop);
}
$("playBtn").addEventListener("click", () => playing ? stop() : start());
$("bypassBtn").addEventListener("click", () => {
  bypass = !bypass; $("bypassBtn").setAttribute("aria-pressed", String(bypass));
  if (node) node.setBypass(bypass);
  if (playing) $("status").textContent = "Playing " + label(current) + (bypass ? " (bypassed)" : "") + ".";
  queueStatic();
});
const SRC_BTNS = [["loop", "srcLoop"], ["tone", "srcTone"], ["sweep", "srcSweep"], ["silence", "srcSilence"], ["file", "srcFile"]];
function selectSource(k, quiet) {
  current = k;
  SRC_BTNS.forEach(([s, id]) => $(id).setAttribute("aria-pressed", String(s === k)));
  if (quiet) return;
  if (playing) start(); else queueStatic();
}
SRC_BTNS.forEach(([s, id]) => s !== "file" && $(id).addEventListener("click", () => selectSource(s)));
$("srcFile").addEventListener("click", () => sources.file && selectSource("file"));
$("loadBtn").addEventListener("click", () => $("fileIn").click());
$("fileIn").addEventListener("change", e => e.target.files[0] && loadFile(e.target.files[0]));
const panel = $("demoControls");
panel.addEventListener("dragover", e => { e.preventDefault(); panel.classList.add("drop-active"); });
panel.addEventListener("dragleave", () => panel.classList.remove("drop-active"));
panel.addEventListener("drop", e => { e.preventDefault(); panel.classList.remove("drop-active"); const f = e.dataTransfer.files[0]; if (f) loadFile(f); });
async function loadFile(f) {
  if (!actx) return;
  $("status").textContent = "Decoding " + f.name + "…";
  try {
    const buf = await actx.decodeAudioData(await f.arrayBuffer());
    const c0 = buf.getChannelData(0), c1 = buf.numberOfChannels > 1 ? buf.getChannelData(1) : c0;
    if (buf.length < N) throw new Error("the file is shorter than 0.2 seconds");
    const lim = Math.min(buf.length, Math.round(4 * buf.sampleRate));
    let best = 0, bestE = -1;
    for (let o = 0; o + N <= lim; o += N) { let e = 0; for (let i = o; i < o + N; i += 8) e += c0[i] * c0[i]; if (e > bestE) { bestE = e; best = o; } }
    sources.file = { chans: [c0, c1], sr: buf.sampleRate, previewAt: best, buffer: buf, name: f.name };
    $("srcFile").disabled = false;
    $("srcFile").textContent = f.name.length > 18 ? f.name.slice(0, 16) + "…" : f.name;
    selectSource("file");
    if (!playing) $("status").textContent = "Loaded " + f.name + ". Press Play.";
  } catch (e) {
    $("status").textContent = "Couldn't read " + f.name + ": " + (e.message || "unsupported format") + ". Try WAV, MP3 or M4A.";
  }
}

$("hostInfo").textContent = "host " + (hostRate / 1000).toFixed(1) + " kHz · " + (actx ? "Web Audio ready" : "no Web Audio");
syncUI();
renderStatic();
new ResizeObserver(() => queueStatic()).observe($("spec"));
})();
