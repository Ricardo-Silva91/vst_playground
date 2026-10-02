// Lizard Tape DSP for the live demo on docs/lizard_tape.html.
// Kept as one source string so the same code runs in the AudioWorklet
// (via a blob: URL) and on the main thread (offline preview). The body must
// not contain backticks or "${".
window.LIZARD_TAPE_DSP = String.raw`// JS port of lizard_tape/Source/DSP/TapeProcessor.h plus the processor's
// wiring. Math.fround mirrors every C++ float op; double members stay plain
// JS numbers. Float literals (0.7f etc.) are pre-rounded constants so they
// carry float precision, not double.
const f = Math.fround;
const F = { f00006: f(0.0006), f005: f(0.05), f07: f(0.7), f03: f(0.3), f025: f(0.25), f075: f(0.75),
            f001: f(0.01), f01: f(0.1), f0001: f(0.001) };
const clamp = (v, lo, hi) => Math.min(hi, Math.max(lo, v));

// std::mt19937 seeded like rng.seed(0xC0FFEEu)
class MT19937 {
  constructor() { this.mt = new Uint32Array(624); this.i = 624; }
  seed(s) {
    const mt = this.mt; mt[0] = s >>> 0;
    for (let i = 1; i < 624; i++) { const p = mt[i - 1] ^ (mt[i - 1] >>> 30); mt[i] = (Math.imul(1812433253, p) + i) >>> 0; }
    this.i = 624;
  }
  next() {
    const mt = this.mt;
    if (this.i >= 624) {
      for (let k = 0; k < 624; k++) {
        const y = (mt[k] & 0x80000000) | (mt[(k + 1) % 624] & 0x7fffffff);
        mt[k] = mt[(k + 397) % 624] ^ (y >>> 1) ^ (y & 1 ? 0x9908b0df : 0);
      }
      this.i = 0;
    }
    let y = mt[this.i++];
    y ^= y >>> 11; y ^= (y << 7) & 0x9d2c5680; y ^= (y << 15) & 0xefc60000; y ^= y >>> 18;
    return y >>> 0;
  }
  // libstdc++ uniform_real_distribution<float>{-1, 1}: generate_canonical<float, 24>
  // takes one 32-bit draw, rounds it to float and divides by 2^32.
  dist() {
    let c = f(this.next()) / 4294967296;
    if (c >= 1) c = f(1 - 5.960464477539063e-8);
    return f(c * 2 - 1);
  }
}

class SmoothedValue {
  constructor() { this.coeff = 0; this.target = 0; this.current = 0; }
  reset(sampleRate, rampSeconds) { const t = Math.max(rampSeconds, 1.0e-5); this.coeff = Math.exp(-1.0 / (t * sampleRate)); this.current = this.target; }
  setTarget(v) { this.target = f(v); }
  snap(v) { this.target = this.current = f(v); }
  next() { this.current = this.target + (this.current - this.target) * this.coeff; return f(this.current); }
}

class FractionalDelay {
  prepare(sampleRate, maxDelayMs) {
    const maxSamples = Math.ceil(maxDelayMs * 0.001 * sampleRate) + 4;
    this.size = Math.max(maxSamples, 8);
    this.buffer = new Float32Array(this.size);
    this.writePos = 0;
  }
  clear() { this.buffer.fill(0); }
  processSample(input, delaySamples) {
    const buf = this.buffer, size = this.size;
    buf[this.writePos] = input;
    delaySamples = clamp(delaySamples, 1, f(size - 3));
    const readPos = f(this.writePos - delaySamples);
    const i1 = Math.floor(readPos);
    const frac = f(readPos - i1);
    const wrap = i => { i %= size; return i < 0 ? i + size : i; };
    const y0 = buf[wrap(i1 - 1)], y1 = buf[wrap(i1)], y2 = buf[wrap(i1 + 1)], y3 = buf[wrap(i1 + 2)];
    const a0 = f(f(f(f(-0.5 * y0) + f(1.5 * y1)) - f(1.5 * y2)) + f(0.5 * y3));
    const a1 = f(f(f(y0 - f(2.5 * y1)) + f(2.0 * y2)) - f(0.5 * y3));
    const a2 = f(f(-0.5 * y0) + f(0.5 * y2));
    const a3 = y1;
    const out = f(f(f(f(f(f(a0 * frac) + a1) * frac) + a2) * frac) + a3);
    if (++this.writePos >= size) this.writePos = 0;
    return out;
  }
}

const TWO_PI = 6.283185307179586;
const dbToGain = db => f(Math.pow(10, f(db * F.f005)));
const SMOOTHED = ["sDrive", "sBias", "sWowDepth", "sWowRate", "sFlutDepth", "sFlutRate", "sTone", "sHiss", "sMix", "sOut"];
const PARAM_OF = { sDrive: "driveDb", sBias: "bias", sWowDepth: "wowDepthMs", sWowRate: "wowRateHz", sFlutDepth: "flutterDepthMs",
                   sFlutRate: "flutterRateHz", sTone: "toneHz", sHiss: "hiss", sMix: "mix", sOut: "outDb" };

class TapeChannel {
  constructor() {
    this.sampleRate = 44100; this.baseDelayMs = 12.0;
    this.delay = new FractionalDelay();
    for (const k of SMOOTHED) this[k] = new SmoothedValue();
    this.wowPhase = 0; this.flutPhase = 0;
    this.lpState = 0; this.hissLpState = 0; this.driftState = 0;
    this.rng = new MT19937(); this.rng.seed(0xC0FFEE);
  }
  prepare(sr) {
    this.sampleRate = sr;
    this.baseDelayMs = 12.0;
    this.delay.prepare(sr, this.baseDelayMs + 30.0);
    this.delay.clear();
    for (const k of SMOOTHED) this[k].reset(sr, 0.02);
    this.wowPhase = this.flutPhase = 0;
    this.lpState = this.hissLpState = this.driftState = 0;
    this.rng.seed(0xC0FFEE);
  }
  setParams(p) { for (const k of SMOOTHED) this[k].setTarget(p[PARAM_OF[k]]); }
  snapParams(p) { for (const k of SMOOTHED) this[k].snap(p[PARAM_OF[k]]); }
  processSample(x) {
    const sr = this.sampleRate, srF = f(sr);
    const dry = x;
    // 1. drive + asymmetric tape saturation
    const driveLin = dbToGain(this.sDrive.next());
    const bias = this.sBias.next();
    let s = f(x * driveLin);
    const hb = f(bias * 0.5);
    s = f(f(Math.tanh(f(s + hb))) - f(Math.tanh(hb)));
    s = f(s * f(1 / Math.max(0.25, f(driveLin * F.f07))));
    // 2. wow & flutter (modulated delay)
    const wowRate = Math.max(F.f001, this.sWowRate.next());
    const flutRate = Math.max(F.f01, this.sFlutRate.next());
    this.wowPhase += TWO_PI * wowRate / sr;
    this.flutPhase += TWO_PI * flutRate / sr;
    if (this.wowPhase > TWO_PI) this.wowPhase -= TWO_PI;
    if (this.flutPhase > TWO_PI) this.flutPhase -= TWO_PI;
    const noise = this.rng.dist();
    this.driftState = f(this.driftState + f(F.f00006 * f(noise - this.driftState)));
    const wowMs = f(this.sWowDepth.next() * f(f(0.5 * f(Math.sin(this.wowPhase))) + f(f(0.5 * this.driftState) * 4)));
    const flutMs = f(this.sFlutDepth.next() * f(Math.sin(this.flutPhase)));
    const delayMs = f(f(f(this.baseDelayMs) + wowMs) + flutMs);
    const delaySamples = f(f(delayMs * F.f0001) * srF);
    s = this.delay.processSample(s, delaySamples);
    // 3. tone: one-pole HF roll-off
    const cutoff = clamp(this.sTone.next(), 500, f(sr * 0.45));
    const g = f(1 - Math.exp(-TWO_PI * cutoff / srF));
    this.lpState = f(this.lpState + f(g * f(s - this.lpState)));
    s = this.lpState;
    // 4. hiss: filtered noise, lightly program-dependent
    const hissAmt = this.sHiss.next();
    if (hissAmt > 0) {
      const n = this.rng.dist();
      this.hissLpState = f(this.hissLpState + f(F.f005 * f(n - this.hissLpState)));
      const bright = f(n - this.hissLpState);
      const floorLvl = f(F.f025 + f(F.f075 * Math.min(1, f(Math.abs(dry) * 4))));
      s = f(s + f(f(f(hissAmt * F.f005) * f(f(F.f07 * this.hissLpState) + f(F.f03 * bright))) * f(0.5 + f(0.5 * floorLvl))));
    }
    // 5. mix + output
    const mix = clamp(this.sMix.next(), 0, 1);
    let out = f(f(dry * f(1 - mix)) + f(s * mix));
    out = f(out * dbToGain(this.sOut.next()));
    return out;
  }
}

// Mirrors LizardTapeAudioProcessor: one TapeChannel per output channel,
// prepareToPlay snaps the parameters, processBlock sets them as smoothing
// targets once per block. p holds the plugin's parameter values; they are
// rounded to float, as the plugin reads them from std::atomic<float>.
const tapeParams = p => ({ driveDb: f(p.drive), bias: f(p.bias), wowDepthMs: f(p.wowDepth), wowRateHz: f(p.wowRate),
  flutterDepthMs: f(p.flutterDepth), flutterRateHz: f(p.flutterRate), toneHz: f(p.tone), hiss: f(p.hiss), mix: f(p.mix), outDb: f(p.output) });

class LizardTape {
  constructor(channels) { this.ch = []; for (let c = 0; c < Math.max(1, channels); c++) this.ch.push(new TapeChannel()); }
  prepareToPlay(sr, p) { const tp = tapeParams(p); for (const c of this.ch) { c.prepare(sr); c.snapParams(tp); } }
  processBlock(bufs, n, p) {
    const tp = tapeParams(p);
    for (const c of this.ch) c.setParams(tp);
    for (let c = 0; c < bufs.length && c < this.ch.length; c++) {
      const x = bufs[c], tc = this.ch[c];
      for (let i = 0; i < n; i++) x[i] = tc.processSample(x[i]);
    }
  }
}
`;
