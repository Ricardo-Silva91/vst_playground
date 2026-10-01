// Live demo for docs/lizard_suite.html. Adapted from the Lizard Suite test bench.
// The DSP itself is in lizard_suite_dsp.js (a JS port of lizard_suite/Source/DSP/*.h).
(function () {
const DSP_SRC = window.LIZARD_SUITE_DSP;
const M = new Function(DSP_SRC + "\nreturn { LizardSuite };")();

// ---------- parameters (ranges, defaults and skews match createParameterLayout) ----------
const skewForCentre = (s, e, c) => Math.log(0.5) / Math.log((c - s) / (e - s));
const skewed = (s, e, k) => ({
  toValue: p => p <= 0 ? s : s + (e - s) * Math.exp(Math.log(p) / k),
  toProp: v => v <= s ? 0 : Math.min(1, Math.exp(Math.log((v - s) / (e - s)) * k)),
});
const lin = (s, e) => skewed(s, e, 1);
const snap = (v, step) => Math.round(v / step) * step;
const fmtHz = f => f >= 1000 ? (f / 1000).toFixed(f >= 10000 ? 1 : 2).replace(/\.?0+$/, "") + " kHz" : Math.round(f) + " Hz";
const pct = v => Math.round(v * 100) + " %";
let hostRate = 48000;

const MODULES = [
  { key: "dust", name: "Dust", role: "sampler", offParam: "dustMix", params: [
    { id: "dustRate", label: "Rate", def: 26040, map: skewed(1000, 48000, skewForCentre(1000, 48000, 12000)), step: 1,
      fmt: v => v >= hostRate ? "OFF ≥ host" : (v / 1000).toFixed(2) + " kHz" },
    { id: "dustBits", label: "Bits", def: 12, int: [2, 16], fmt: v => v + " bit" },
    { id: "dustLowpass", label: "Low-pass", def: 0, map: skewed(0, 20000, skewForCentre(0, 20000, 4000)), step: 1,
      fmt: v => v <= 0 ? "OFF" : v >= hostRate / 2 ? "OFF ≥ Nyq" : (v / 1000).toFixed(1) + " kHz" },
    { id: "dustDrive", label: "Drive", def: 1, map: skewed(1, 8, 0.5), step: 0.01, fmt: v => "×" + v.toFixed(2) },
    { id: "dustMix", label: "Mix", def: 1, map: lin(0, 1), step: 0.001, fmt: pct },
  ] },
  { key: "chew", name: "Chew", role: "tape", offParam: "chewDepth", params: [
    { id: "chewRate", label: "Rate", def: 3, map: skewed(0, 20, skewForCentre(0, 20, 4)), step: 0.01, fmt: v => v.toFixed(1) + " /s" },
    { id: "chewDepth", label: "Depth", def: 0.5, map: lin(0, 1), step: 0.001, fmt: pct },
    { id: "chewSmooth", label: "Smooth", def: 8, map: skewed(1, 50, skewForCentre(1, 50, 10)), step: 0.1, fmt: v => v.toFixed(1) + " ms" },
  ] },
  { key: "murk", name: "Murk", role: "room", offParam: "murkMix", params: [
    { id: "murkRoom", label: "Room", def: 0.6, map: lin(0, 1), step: 0.001, fmt: pct },
    { id: "murkDamp", label: "Damp", def: 0.5, map: lin(0, 1), step: 0.001, fmt: pct },
    { id: "murkMix", label: "Mix", def: 0.3, map: lin(0, 1), step: 0.001, fmt: pct },
  ] },
  { key: "vinyl", name: "Vinyl", role: "record", offParam: "vinylAmount", params: [
    { id: "vinylAge", label: "Age", def: 0.4, map: lin(0, 1), step: 0.001, fmt: pct },
    { id: "vinylAmount", label: "Amount", def: 0.5, map: lin(0, 1), step: 0.001, fmt: pct },
    { id: "vinylSeed", label: "Seed", def: 1, int: [1, 999], fmt: v => "#" + v },
  ] },
];
const PDEF = {};
MODULES.forEach(m => m.params.forEach(p => { PDEF[p.id] = p; }));
const DEFAULTS = Object.fromEntries(Object.values(PDEF).map(p => [p.id, p.def]));
const P = { ...DEFAULTS };
const OFF = { dustMix: 0, chewDepth: 0, murkMix: 0, vinylAmount: 0 };

const PRESETS = [
  { name: "Dead tape", p: { dustRate: 26040, dustBits: 12, dustLowpass: 9000, dustDrive: 1.3, dustMix: 1, chewRate: 4, chewDepth: 0.6, chewSmooth: 10, murkRoom: 0.7, murkDamp: 0.75, murkMix: 0.3, vinylAge: 0.6, vinylAmount: 0.6 } },
  { name: "Plugin defaults", p: { ...DEFAULTS } },
  { name: "Basement", p: { ...OFF, murkRoom: 0.85, murkDamp: 0.9, murkMix: 0.55 } },
  { name: "Chewed cassette", p: { ...OFF, chewRate: 8, chewDepth: 0.9, chewSmooth: 6, dustRate: 32000, dustBits: 12, dustLowpass: 7000, dustMix: 1 } },
  { name: "Old 78", p: { ...OFF, dustRate: 16000, dustBits: 10, dustLowpass: 4500, dustDrive: 1.6, dustMix: 1, vinylAge: 0.95, vinylAmount: 0.85 } },
  { name: "Clean", p: { ...OFF } },
];

const $ = id => document.getElementById(id);
let actx = null;
try { actx = new (window.AudioContext || window.webkitAudioContext)(); hostRate = actx.sampleRate; } catch (e) { actx = null; }

// ---------- rack UI ----------
MODULES.forEach((m, i) => {
  const el = document.createElement("div"); el.className = "lz-module"; el.setAttribute("role", "group"); el.setAttribute("aria-label", m.name);
  let h = '<div class="lz-mod-head"><span class="lz-mod-num">0' + (i + 1) + '</span><h3 class="lz-mod-name">' + m.name + '</h3><span class="lz-mod-role">' + m.role + '</span><span class="lz-mod-state" id="' + m.key + 'State">ON</span></div>';
  for (const p of m.params) {
    const max = p.int ? p.int[1] : 10000, min = p.int ? p.int[0] : 0;
    h += '<div class="lz-ctl"><label for="' + p.id + '">' + p.label + '</label><output class="lz-lcd" id="' + p.id + 'Out" for="' + p.id + '"></output>' +
      '<input id="' + p.id + '" type="range" min="' + min + '" max="' + max + '" step="1"></div>';
  }
  h += '<dl class="lz-stats" id="' + m.key + 'Stats"></dl>';
  el.innerHTML = h;
  $("rack").appendChild(el);
});
Object.values(PDEF).forEach(p => {
  $(p.id).addEventListener("input", e => {
    const v = +e.target.value;
    P[p.id] = p.int ? v : snap(p.map.toValue(v / 10000), p.step);
    if (p.id === "dustLowpass" && P[p.id] < 20) P[p.id] = 0;
    changed();
  });
});

function stat(dt, dd) { return '<div class="lz-stat"><dt>' + dt + '</dt><dd>' + dd + '</dd></div>'; }
const dB = v => v <= 0 ? "−∞ dB" : (20 * Math.log10(v)).toFixed(0).replace("-", "−") + " dBFS";
function derived() {
  const sr = hostRate, ratio = Math.min(1, P.dustRate / sr);
  const s = {};
  s.dust = stat("Hold length", ratio >= 1 ? "off" : (1 / ratio).toFixed(2) + " smp") + stat("Reduced Nyquist", ratio >= 1 ? "—" : fmtHz(P.dustRate / 2)) +
    stat("Quantiser", ((1 << P.dustBits) - 1).toLocaleString("en-US") + " steps") + stat("Dynamic range", "≈ " + (6.02 * P.dustBits).toFixed(0) + " dB");
  const perSample = P.chewRate / sr;
  s.chew = stat("Dropout chance", perSample > 0 ? "1 in " + Math.round(1 / perSample).toLocaleString("en-US") + " smp" : "never") +
    stat("Mean gap", P.chewRate > 0 ? (1000 / P.chewRate).toFixed(0) + " ms" : "—") +
    stat("Deepest sag", P.chewDepth > 0 ? dB(1 - P.chewDepth).replace(" dBFS", " dB") : "0 dB") + stat("Recovery", "≈ 50 ms");
  const fb = 0.70 + 0.28 * P.murkRoom, dmp = 0.20 + 0.75 * P.murkDamp;
  const meanLen = (1116 + 1188 + 1277 + 1356) / 4 / 44100; // seconds, rate independent
  const rt60 = 3 * meanLen / -Math.log10(fb);
  const lpCut = -Math.log(dmp) * sr / (2 * Math.PI);
  s.murk = stat("Comb feedback", fb.toFixed(3)) + stat("Low decay (RT60)", rt60.toFixed(1) + " s") +
    stat("Damping pole", dmp.toFixed(2)) + stat("Loop low-pass", "≈ " + fmtHz(lpCut));
  const hiss = (0.0008 + 0.006 * P.vinylAge) * P.vinylAmount, rum = (0.002 + 0.02 * P.vinylAge) * 4 * P.vinylAmount;
  const clicks = (0.0006 + 0.02 * P.vinylAge) * (sr / 44100) * 0.5 * sr;
  s.vinyl = stat("Hiss peak", dB(hiss)) + stat("Rumble peak", dB(rum)) +
    stat("Pops", P.vinylAmount > 0 ? "≈ " + Math.round(clicks).toLocaleString("en-US") + " /s" : "none") + stat("Pop peak", dB((0.2 + 0.8 * P.vinylAge) * P.vinylAmount));
  return s;
}

function syncUI() {
  for (const p of Object.values(PDEF)) {
    $(p.id).value = p.int ? P[p.id] : Math.round(p.map.toProp(P[p.id]) * 10000);
    const t = p.fmt(P[p.id]); $(p.id + "Out").textContent = t; $(p.id).setAttribute("aria-valuetext", t);
  }
  const d = derived();
  for (const m of MODULES) {
    $(m.key + "Stats").innerHTML = d[m.key];
    const off = P[m.offParam] <= 0, st = $(m.key + "State");
    st.textContent = off ? "OFF · dry" : "ON"; st.classList.toggle("off", off);
  }
  document.querySelectorAll("#lzPresets button").forEach((b, i) => {
    const pp = { ...P, ...PRESETS[i].p };
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
function makeSilence(sr) { const X = new Float32Array(Math.round(4 * sr)); return { chans: [X, X], sr, previewAt: Math.round(sr) }; }
const sources = { loop: makeLoop(hostRate), sweep: makeSweep(hostRate), silence: makeSilence(hostRate), file: null };
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
  const dustOn = P.dustMix > 0, nyq = P.dustRate / 2;
  if (dustOn && nyq < fmax) { g.fillStyle = css("--shade"); g.fillRect(xOf(nyq), pt, pl + pw - xOf(nyq), ph); }
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
  const dry = path(pDry), wet = path(pWet);
  g.beginPath(); g.moveTo(pl, pt + ph); wet.forEach(([x, y]) => g.lineTo(x, y)); g.lineTo(pl + pw, pt + ph); g.closePath();
  g.globalAlpha = 0.22; g.fillStyle = css("--accent"); g.fill(); g.globalAlpha = 1;
  g.lineWidth = 1.25; g.strokeStyle = css("--dry"); g.beginPath(); dry.forEach(([x, y], i) => i ? g.lineTo(x, y) : g.moveTo(x, y)); g.stroke();
  g.lineWidth = 1.5; g.strokeStyle = css("--accent"); g.beginPath(); wet.forEach(([x, y], i) => i ? g.lineTo(x, y) : g.moveTo(x, y)); g.stroke();
  if (dustOn && nyq < fmax) marker(g, xOf(nyq), "½ Dust rate " + fmtHz(nyq), [5, 4], css("--marker"), pt + 10, pl, pw, pt, ph);
  if (dustOn && P.dustLowpass > 0 && P.dustLowpass < sr / 2) marker(g, xOf(P.dustLowpass), "LP " + fmtHz(P.dustLowpass), [2, 3], css("--muted"), pt + 26, pl, pw, pt + 18, ph - 18);
  if (P.vinylAmount > 0) marker(g, xOf(50), "rumble 50 Hz", [2, 3], css("--muted"), pt + 42, pl, pw, pt + 34, ph - 34);
}
const SCOPE_N = 160;
function drawScope(dry, wet, off) {
  const { g, W, H } = fit($("scope"));
  const pl = 8, pr = 8, pt = 10, pb = 10, pw = W - pl - pr, ph = H - pt - pb;
  let peak = 0.02; for (let i = 0; i < SCOPE_N; i++) peak = Math.max(peak, Math.abs(dry[off + i]), Math.abs(wet[off + i]));
  const sc = 1 / Math.min(1, peak * 1.1);
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
function drawLevel(rDry, rWet, secs) {
  const { g, W, H } = fit($("level"));
  const pl = 38, pr = 10, pt = 10, pb = 22, pw = W - pl - pr, ph = H - pt - pb;
  const yOf = db => pt + Math.min(1, Math.max(0, -db / 90)) * ph;
  const toDb = v => 20 * Math.log10(v + 1e-9);
  g.clearRect(0, 0, W, H); g.font = "11px " + css("--mono"); g.textBaseline = "middle";
  g.strokeStyle = css("--grid"); g.fillStyle = css("--muted"); g.lineWidth = 1;
  for (let db = -15; db >= -90; db -= 15) { const y = Math.round(yOf(db)) + 0.5; g.beginPath(); g.moveTo(pl, y); g.lineTo(pl + pw, y); g.stroke(); if (db % 30 === 0) { g.textAlign = "right"; g.fillText(db, pl - 6, y); } }
  g.textAlign = "center";
  for (let s = 0; s <= secs; s++) { const x = Math.round(pl + s / secs * pw) + 0.5; g.beginPath(); g.moveTo(x, pt); g.lineTo(x, pt + ph); g.stroke(); g.fillText(s + " s", Math.min(W - 16, Math.max(pl + 8, x)), H - 10); }
  const line = (r, color, w, fill) => {
    g.beginPath(); for (let i = 0; i < r.length; i++) { const x = pl + (i + 0.5) / r.length * pw, y = yOf(toDb(r[i])); i ? g.lineTo(x, y) : g.moveTo(x, y); }
    if (fill) { g.lineTo(pl + pw, pt + ph); g.lineTo(pl, pt + ph); g.closePath(); g.globalAlpha = 0.2; g.fillStyle = color; g.fill(); g.globalAlpha = 1; return; }
    g.lineWidth = w; g.strokeStyle = color; g.stroke();
  };
  line(rWet, css("--accent"), 0, true);
  line(rDry, css("--dry"), 1.25);
  line(rWet, css("--accent"), 1.5);
}

// ---------- static preview (the same port, run offline) ----------
const pD = new Float64Array(N / 2), pW = new Float64Array(N / 2), LEVEL_S = 4;
function renderStatic() {
  const src = sources[current] || sources.loop, sr = src.sr;
  const len = Math.min(src.chans[0].length, Math.round(LEVEL_S * sr));
  const start = Math.max(0, Math.min(src.previewAt, len - N));
  const dry = src.chans[0].subarray(0, len), wet = Float32Array.from(dry);
  if (!bypass) {
    const s = new M.LizardSuite(1); s.prepareToPlay(sr, P);
    for (let o = 0; o < len; o += 128) s.processBlock([wet.subarray(o, Math.min(len, o + 128))], Math.min(128, len - o), P);
  }
  power(dry.subarray(start, start + N), pD); power(wet.subarray(start, start + N), pW);
  drawSpectrum(pD, pW, sr);
  drawScope(dry, wet, start + 1024);
  const win = Math.round(0.01 * sr), bins = Math.floor(Math.round(LEVEL_S * sr) / win);
  const rD = new Float64Array(bins), rW = new Float64Array(bins);
  for (let b = 0; b < bins; b++) { let a = 0, c = 0; const o = b * win;
    for (let i = o; i < o + win && i < len; i++) { a += dry[i] * dry[i]; c += wet[i] * wet[i]; }
    rD[b] = Math.sqrt(a / win); rW[b] = Math.sqrt(c / win); }
  drawLevel(rD, rW, LEVEL_S);
  lastLevel = [rD, rW];
}
let lastLevel = null, staticQueued = false;
function queueStatic() {
  if (staticQueued) return;
  staticQueued = true;
  requestAnimationFrame(() => { staticQueued = false; if (playing) renderLevelOnly(); else renderStatic(); });
}
function renderLevelOnly() {
  // While playing, the live analysers own the spectrum and scope; the level
  // plot still follows the controls from an offline render.
  const s1 = $("spec"), s2 = $("scope");
  const a = s1.getContext("2d").getImageData(0, 0, s1.width, s1.height), b = s2.getContext("2d").getImageData(0, 0, s2.width, s2.height);
  renderStatic();
  s1.getContext("2d").putImageData(a, 0, 0); s2.getContext("2d").putImageData(b, 0, 0);
}

document.querySelectorAll("#lizardDemo [data-try]").forEach(b => b.addEventListener("click", () => {
  const t = b.dataset.try;
  if (t === "off") { Object.assign(P, { dustRate: 4000, dustBits: 4, dustDrive: 8, chewRate: 20, murkRoom: 1, vinylAge: 1 }, OFF); }
  if (t === "murk") { Object.assign(P, DEFAULTS, OFF, { murkRoom: 1, murkDamp: 0, murkMix: 1 }); selectSource("loop", true); }
  if (t === "chew") { Object.assign(P, DEFAULTS, OFF, { chewRate: 8, chewDepth: 1, chewSmooth: 5 }); selectSource("loop", true); }
  if (t === "vinyl") { Object.assign(P, DEFAULTS, OFF, { vinylAge: 0.5, vinylAmount: 1 }); selectSource("silence", true); }
  if (t === "dead") { Object.assign(P, DEFAULTS, PRESETS[0].p); selectSource("loop", true); }
  changed();
  start(); // restarts with the card's source if one is already playing
}));

// ---------- audio graph ----------
let node = null, sendParams = () => {}, pre = null, post = null, srcNode = null, playing = false, bypass = false, graphReady = null;
const WORKLET = DSP_SRC + "\n" + String.raw`
class LSProc extends AudioWorkletProcessor {
  constructor() {
    super(); this.s = new LizardSuite(2); this.p = null; this.bypass = false;
    this.port.onmessage = e => { const m = e.data;
      if (m.type === "params") this.p = m.p;
      else if (m.type === "bypass") this.bypass = m.on;
      else if (m.type === "reset") { this.p = m.p; this.s.prepareToPlay(sampleRate, m.p); } };
  }
  process(inputs, outputs) {
    const inp = inputs[0], out = outputs[0];
    for (let ch = 0; ch < out.length; ch++) {
      if (!inp || inp.length === 0) out[ch].fill(0); else out[ch].set(inp[Math.min(ch, inp.length - 1)]);
    }
    if (!this.bypass && this.p) this.s.processBlock(out, out[0].length, this.p);
    return true;
  }
}
registerProcessor("lizard-suite", LSProc);`;

async function buildGraph() {
  pre = actx.createAnalyser(); pre.fftSize = N; pre.smoothingTimeConstant = 0;
  post = actx.createAnalyser(); post.fftSize = N; post.smoothingTimeConstant = 0;
  const out = actx.createGain(); out.gain.value = 0.9;
  try {
    const url = URL.createObjectURL(new Blob([WORKLET], { type: "application/javascript" }));
    await actx.audioWorklet.addModule(url);
    node = new AudioWorkletNode(actx, "lizard-suite", { numberOfInputs: 1, numberOfOutputs: 1, outputChannelCount: [2] });
    sendParams = () => node.port.postMessage({ type: "params", p: { ...P } });
    node.setBypass = on => node.port.postMessage({ type: "bypass", on });
    node.resetDsp = () => node.port.postMessage({ type: "reset", p: { ...P } });
  } catch (e) {
    node = actx.createScriptProcessor(1024, 2, 2);
    const suite = new M.LizardSuite(2); let by = false;
    node.onaudioprocess = ev => {
      const o = [0, 1].map(ch => { const b = ev.outputBuffer.getChannelData(ch); b.set(ev.inputBuffer.getChannelData(Math.min(ch, ev.inputBuffer.numberOfChannels - 1))); return b; });
      if (by) return;
      for (let k = 0; k < o[0].length; k += 128) suite.processBlock(o.map(b => b.subarray(k, k + 128)), Math.min(128, o[0].length - k), P);
    };
    sendParams = () => {};
    node.setBypass = on => { by = on; };
    node.resetDsp = () => suite.prepareToPlay(actx.sampleRate, P);
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
const label = k => ({ loop: "the drum loop", sweep: "the 20 Hz – 20 kHz sweep", silence: "silence" })[k] || (sources.file && sources.file.name) || "your file";

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
function selectSource(k, quiet) {
  current = k;
  [["loop", "srcLoop"], ["sweep", "srcSweep"], ["silence", "srcSilence"], ["file", "srcFile"]].forEach(([s, id]) => $(id).setAttribute("aria-pressed", String(s === k)));
  if (quiet) return;
  if (playing) start(); else queueStatic();
}
$("srcLoop").addEventListener("click", () => selectSource("loop"));
$("srcSweep").addEventListener("click", () => selectSource("sweep"));
$("srcSilence").addEventListener("click", () => selectSource("silence"));
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
    const lim = Math.min(buf.length, Math.round(LEVEL_S * buf.sampleRate));
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
matchMedia("(prefers-color-scheme: dark)").addEventListener("change", queueStatic);
new MutationObserver(queueStatic).observe(document.documentElement, { attributes: true, attributeFilter: ["data-theme"] });
})();
