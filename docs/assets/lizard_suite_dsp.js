// Lizard Suite DSP for the live demo on docs/lizard_suite.html.
// Kept as one source string so the same code runs in the AudioWorklet
// (via a blob: URL) and on the main thread (offline preview). The body must
// not contain backticks or "${".
window.LIZARD_SUITE_DSP = String.raw`// wiring in LizardSuiteProcessor. Math.fround mirrors every C++ float op;
// double members stay plain JS numbers. Float literals (0.6f etc.) are
// pre-rounded constants so they carry float precision, not double.
const f = Math.fround;
const F = { f02: f(0.2), f06: f(0.6), f08: f(0.8), f070: f(0.70), f028: f(0.28), f075: f(0.75) };
const clamp = (v, lo, hi) => Math.min(hi, Math.max(lo, v));

// xorshift32 -> [0,1), shared by Vinyl and Chew (same frand() in both headers)
function xorshift(self) {
  let r = self.rng;
  r = (r ^ (r << 13)) >>> 0; r = (r ^ (r >>> 17)) >>> 0; r = (r ^ (r << 5)) >>> 0;
  self.rng = r;
  return (r & 0xFFFFFF) / 16777216; // exact in float
}

class LizardDust {
  constructor() {
    this.hostRate = 44100; this.targetRate = 26040; this.bits = 12;
    this.lpHz = 0; this.drive = 1; this.mix = 1;
    this.hold = 0; this.phase = 1; this.lpZ = 0;
  }
  setHostSampleRate(sr) { this.hostRate = sr > 0 ? sr : 44100; }
  setTargetRate(hz) { this.targetRate = hz > 0 ? hz : this.hostRate; }
  setBitDepth(b) { this.bits = Math.min(16, Math.max(2, b | 0)); }
  setLowpassHz(hz) { this.lpHz = hz; }
  setDrive(d) { this.drive = Math.max(0.0001, d); }
  setMix(m) { this.mix = clamp(m, 0, 1); }
  reset() { this.hold = 0; this.phase = 1; this.lpZ = 0; }
  process(x, n) {
    const ratio = clamp(this.targetRate / this.hostRate, 1e-4, 1);
    const levels = f((1 << this.bits) - 1), invLev = f(1 / levels);
    const useLp = this.lpHz > 0 && this.lpHz < this.hostRate * 0.5;
    const a = useLp ? f(Math.exp(-2 * Math.PI * (this.lpHz / this.hostRate))) : 0;
    const oneMinusA = f(1 - a);
    const useDrive = this.drive !== 1;
    const drv = f(this.drive), tanhDrv = f(Math.tanh(drv)), mix = f(this.mix);
    let hold = this.hold, phase = this.phase, lpZ = this.lpZ;
    for (let i = 0; i < n; i++) {
      const dry = x[i];
      phase += ratio;
      if (phase >= 1) { phase -= 1; hold = dry; }
      let s = hold;
      if (useDrive) s = f(f(Math.tanh(f(s * drv))) / tanhDrv);
      let c = s < -1 ? -1 : (s > 1 ? 1 : s);
      c = f(f(f(Math.round(f(f(f(c * 0.5) + 0.5) * levels)) * invLev) * 2) - 1);
      if (useLp) { lpZ = f(f(oneMinusA * c) + f(a * lpZ)); c = lpZ; }
      x[i] = f(dry + f(f(c - dry) * mix));
    }
    this.hold = hold; this.phase = phase; this.lpZ = lpZ;
  }
}

class LizardChew {
  constructor() {
    this.sr = 44100; this.rate = 3; this.depth = 0.5; this.smoothMs = 8;
    this.rng = 99991; this.gain = 1; this.target = 1;
  }
  setHostSampleRate(sr) { this.sr = sr > 0 ? sr : 44100; }
  setRate(r) { this.rate = Math.max(0, r); }
  setDepth(d) { this.depth = clamp(d, 0, 1); }
  setSmoothMs(ms) { this.smoothMs = Math.max(0.1, ms); }
  setSeed(s) { s = s >>> 0; this.rng = s ? s : 1; }
  reset() { this.gain = 1; this.target = 1; }
  process(x, n) {
    if (this.depth <= 0) return;
    const p = this.rate / this.sr;
    const sm = f(Math.exp(-1 / (0.001 * this.smoothMs * this.sr)));
    const rec = f(Math.exp(-1 / (0.05 * this.sr)));
    const depth = f(this.depth);
    let gain = this.gain, target = this.target;
    for (let i = 0; i < n; i++) {
      if (xorshift(this) < p)
        target = f(1 - f(depth * xorshift(this)));
      target = f(1 + f(f(target - 1) * rec));
      gain = f(target + f(f(gain - target) * sm));
      x[i] = f(x[i] * gain);
    }
    this.gain = gain; this.target = target;
  }
}

class LizardMurk {
  constructor() {
    this.sr = 44100; this.room = 0.6; this.damp = 0.5; this.mix = 0.3;
    this.rebuild();
  }
  setHostSampleRate(sr) { this.sr = sr > 0 ? sr : 44100; this.rebuild(); }
  setRoomSize(r) { this.room = clamp(r, 0, 1); }
  setDamp(d) { this.damp = clamp(d, 0, 1); }
  setMix(m) { this.mix = clamp(m, 0, 1); }
  reset() {
    for (const c of this.combs) { c.buf.fill(0); c.idx = 0; c.store = 0; }
    for (const a of this.alls) { a.buf.fill(0); a.idx = 0; }
  }
  rebuild() {
    const k = this.sr / 44100;
    const cl = [1116, 1188, 1277, 1356], al = [556, 441];
    this.combs = cl.map(l => ({ buf: new Float32Array(Math.max(1, Math.round(l * k))), idx: 0, store: 0, dmp: f(0.5) }));
    this.alls = al.map(l => ({ buf: new Float32Array(Math.max(1, Math.round(l * k))), idx: 0 }));
  }
  process(x, n) {
    if (this.mix <= 0) return;
    const fb = f(F.f070 + f(F.f028 * f(this.room)));
    const dmp = f(F.f02 + f(F.f075 * f(this.damp)));
    const oneMinusDmp = f(1 - dmp), mix = f(this.mix);
    for (const c of this.combs) c.dmp = dmp;
    const combs = this.combs, alls = this.alls;
    for (let i = 0; i < n; i++) {
      const xin = f(x[i] * F.f02);
      let acc = 0;
      for (let j = 0; j < 4; j++) {
        const c = combs[j];
        const y = c.buf[c.idx];
        c.store = f(f(y * oneMinusDmp) + f(c.store * dmp));
        c.buf[c.idx] = f(xin + f(c.store * fb));
        if (++c.idx >= c.buf.length) c.idx = 0;
        acc = f(acc + y);
      }
      for (let j = 0; j < 2; j++) {
        const a = alls[j];
        const y = a.buf[a.idx];
        const out = f(-acc + y);
        a.buf[a.idx] = f(acc + f(y * 0.5));
        if (++a.idx >= a.buf.length) a.idx = 0;
        acc = out;
      }
      x[i] = f(x[i] + f(f(acc - x[i]) * mix));
    }
  }
}

class LizardVinyl {
  constructor() {
    this.sr = 44100; this.age = 0.4; this.amount = 0.5;
    this.rng = 22222; this.hissZ = 0; this.rumbleZ = 0; this.clickAmp = 0;
  }
  setHostSampleRate(sr) { this.sr = sr > 0 ? sr : 44100; }
  setAge(a) { this.age = clamp(a, 0, 1); }
  setAmount(m) { this.amount = clamp(m, 0, 1); }
  setSeed(s) { s = s >>> 0; this.rng = s ? s : 1; }
  reset() { this.hissZ = 0; this.rumbleZ = 0; this.clickAmp = 0; }
  process(x, n) {
    if (this.amount <= 0) return;
    const hissLvl = f(0.0008 + 0.006 * this.age);
    const rumbleLvl = f(0.002 + 0.02 * this.age);
    const clickProb = (0.0006 + 0.02 * this.age) * (this.sr / 44100) * 0.5;
    const hissA = F.f06, oneMinusHissA = f(1 - hissA);
    const rumbleA = f(Math.exp(-2 * Math.PI * (50 / this.sr))), oneMinusRumbleA = f(1 - rumbleA);
    const clickScale = f(F.f02 + f(F.f08 * f(this.age)));
    const amount = f(this.amount);
    let hissZ = this.hissZ, rumbleZ = this.rumbleZ, clickAmp = this.clickAmp;
    for (let i = 0; i < n; i++) {
      const w1 = f(xorshift(this) * 2 - 1);
      hissZ = f(f(oneMinusHissA * w1) + f(hissA * hissZ));
      const hiss = f(hissZ * hissLvl);

      const w2 = f(xorshift(this) * 2 - 1);
      rumbleZ = f(f(oneMinusRumbleA * w2) + f(rumbleA * rumbleZ));
      const rumble = f(f(rumbleZ * rumbleLvl) * 4);

      if (xorshift(this) < clickProb)
        clickAmp = f(f(xorshift(this) * 2 - 1) * clickScale);
      const click = clickAmp;
      clickAmp = f(clickAmp * F.f06);

      x[i] = f(x[i] + f(f(f(hiss + rumble) + click) * amount));
    }
    this.hissZ = hissZ; this.rumbleZ = rumbleZ; this.clickAmp = clickAmp;
  }
}

// Mirrors LizardSuiteProcessor: one instance of each core per channel,
// Dust -> Chew -> Murk -> Vinyl, parameters applied once per block. p holds
// the plugin's parameter values; they are rounded to float first, as the
// plugin reads them from std::atomic<float>.
const CHEW_SEED = 99991;
const vinylSeedFor = (seed, ch) => (Math.imul(seed, 2654435761 | 0) + ch * 40503) >>> 0;

class LizardSuite {
  constructor(channels) {
    this.ch = [];
    for (let c = 0; c < channels; c++)
      this.ch.push({ dust: new LizardDust(), chew: new LizardChew(), murk: new LizardMurk(), vinyl: new LizardVinyl() });
    this.lastVinylSeed = -1;
  }
  reseed(seed) {
    this.lastVinylSeed = seed;
    this.ch.forEach((m, c) => { m.chew.setSeed(CHEW_SEED); m.vinyl.setSeed(vinylSeedFor(seed, c)); });
  }
  prepareToPlay(sr, p) {
    for (const m of this.ch) {
      m.dust.setHostSampleRate(sr); m.dust.reset();
      m.chew.setHostSampleRate(sr); m.chew.reset();
      m.murk.setHostSampleRate(sr); m.murk.reset();
      m.vinyl.setHostSampleRate(sr); m.vinyl.reset();
    }
    this.reseed(Math.round(f(p.vinylSeed)));
  }
  processBlock(bufs, n, p) {
    const g = k => f(p[k]);
    if (Math.round(g("vinylSeed")) !== this.lastVinylSeed) this.reseed(Math.round(g("vinylSeed")));
    for (let c = 0; c < bufs.length && c < this.ch.length; c++) {
      const m = this.ch[c], x = bufs[c];
      m.dust.setTargetRate(g("dustRate")); m.dust.setBitDepth(Math.round(g("dustBits")));
      m.dust.setLowpassHz(g("dustLowpass")); m.dust.setDrive(g("dustDrive")); m.dust.setMix(g("dustMix"));
      m.dust.process(x, n);
      m.chew.setRate(g("chewRate")); m.chew.setDepth(g("chewDepth")); m.chew.setSmoothMs(g("chewSmooth"));
      m.chew.process(x, n);
      m.murk.setRoomSize(g("murkRoom")); m.murk.setDamp(g("murkDamp")); m.murk.setMix(g("murkMix"));
      m.murk.process(x, n);
      m.vinyl.setAge(g("vinylAge")); m.vinyl.setAmount(g("vinylAmount"));
      m.vinyl.process(x, n);
    }
  }
}
`;
