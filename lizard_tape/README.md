# Lizard Tape

A lo-fi **tape/cassette** effect for the Lizard JUCE family. Wow & flutter,
asymmetric tape saturation, HF roll-off tone, and program-dependent hiss — the
"ran it through a worn-out Walkman" character, with real cubic-interpolated pitch
wobble rather than a static chorus fake.

## Files
- `CMakeLists.txt` — JUCE plugin (VST3), using the repo's shared `../JUCE` submodule
- `Source/DSP/TapeProcessor.h` — the whole DSP core, JUCE-INDEPENDENT, header-only
- `Source/PluginProcessor.*` — AudioProcessor + APVTS parameter layout
- `Source/PluginEditor.*` — 10-knob hand-laid editor
- `test/test_tape.cpp` — standalone unit tests, no JUCE needed

## Why the DSP is framework-free
All DSP lives in `TapeProcessor.h` with zero JUCE dependencies (just `<cmath>`,
`<vector>`, `<random>`). The maths compiles and tests in a second with plain g++,
no DAW, no plugin host:

```
cd test
g++ -std=c++17 -O2 -Wall test_tape.cpp -o t && ./t
```

Tests confirm: finite & bounded under heavy drive, silence in -> only a low hiss
floor out, wow/flutter genuinely modulates pitch (zero-crossing interval variance
jumps ~20x), and `mix=0` passes dry audio through bit-identical. All pass.

The **Run Tests** workflow runs this test on every PR, along with the JUCE-level
suite in `tests/lizard_tape_test.cpp` (parameters, mix-0 dry path through the real
processor, state save/restore).

## Building the plugin
Builds run on GitHub Actions (**Build VST**): every push to `main` that touches
`lizard_tape/` builds the VST3 for macOS and Windows and publishes it as the
`lizard_tape-latest` release.

## Signal chain
1. **Drive + Bias** — asymmetric tanh saturation. Bias offsets the operating point
   so the curve isn't odd-symmetric, adding even harmonics (warm) over the odd grit.
2. **Wow & Flutter** — modulated fractional delay (Catmull-Rom cubic) on a ~12 ms
   base delay. Slow wow + fast flutter LFOs plus a low-passed random-walk drift so
   the wobble feels mechanical. Cubic interpolation avoids smearing/aliasing the warble.
3. **Tone** — one-pole low-pass modelling tape HF roll-off.
4. **Hiss** — brownish + a little white, lightly ducked by program level.
5. **Mix / Output**.

## Parameters
Drive 0-24 dB (def 4) · Bias 0-1 (0.15) · Wow Depth 0-10 ms (3) · Wow Rate 0.05-4 Hz (0.7)
· Flutter Depth 0-3 ms (0.6) · Flutter Rate 2-16 Hz (8) · Tone 800-18000 Hz (6500)
· Hiss 0-1 (0.06) · Mix 0-1 (1.0) · Output -24..+12 dB (0).

## Where to take it next
- Head-bump EQ (low-mid resonance ~60-120 Hz) for the tape thump.
- Dropouts: slow amplitude/HF dips gated by a slow random source.
- Oversample the saturator (2-4x) to tame aliasing at high drive.
