# CLAUDE.md

Behavioral guidelines to reduce common LLM coding mistakes. Merge with project-specific instructions as needed.

**Tradeoff:** These guidelines bias toward caution over speed. For trivial tasks, use judgment.

## 1. Think Before Coding

**Don't assume. Don't hide confusion. Surface tradeoffs.**

Before implementing:
- State your assumptions explicitly. If uncertain, ask.
- If multiple interpretations exist, present them - don't pick silently.
- If a simpler approach exists, say so. Push back when warranted.
- If something is unclear, stop. Name what's confusing. Ask.

## 2. Simplicity First

**Minimum code that solves the problem. Nothing speculative.**

- No features beyond what was asked.
- No abstractions for single-use code.
- No "flexibility" or "configurability" that wasn't requested.
- No error handling for impossible scenarios.
- If you write 200 lines and it could be 50, rewrite it.

Ask yourself: "Would a senior engineer say this is overcomplicated?" If yes, simplify.

## 3. Surgical Changes

**Touch only what you must. Clean up only your own mess.**

When editing existing code:
- Don't "improve" adjacent code, comments, or formatting.
- Don't refactor things that aren't broken.
- Match existing style, even if you'd do it differently.
- If you notice unrelated dead code, mention it - don't delete it.
- If you notice adjacent code that contradicts the change you're making — even slightly — surface it before implementing.

When your changes create orphans:
- Remove imports/variables/functions that YOUR changes made unused.
- Don't remove pre-existing dead code unless asked.

The test: Every changed line should trace directly to the user's request.

## 4. Goal-Driven Execution

**Define success criteria. Loop until verified.**

Transform tasks into verifiable goals:
- "Add validation" → "Write tests for invalid inputs, then make them pass"
- "Fix the bug" → "Write a test that reproduces it, then make it pass"
- "Refactor X" → "Ensure tests pass before and after"

For multi-step tasks, state a brief plan:
```
1. [Step] → verify: [check]
2. [Step] → verify: [check]
3. [Step] → verify: [check]
```

Strong success criteria let you loop independently. Weak criteria ("make it work") require constant clarification.

## 5. Memory of Agreements

**Once an assumption is confirmed, it is binding for the session.**

- If a previous answer established a convention (debounce timing, naming pattern, error format), apply it to all subsequent related work without re-asking.
- If you need to break a previous agreement, state which one you're breaking and why, before implementing.
- At session start, surface any unresolved decisions from prior turns.

---

**These guidelines are working if:** fewer unnecessary changes in diffs, fewer rewrites due to overcomplication, and clarifying questions come before implementation rather than after mistakes.

---

## Project: vst_playground

A collection of JUCE/CMake VST3 plugins. Builds run exclusively on GitHub Actions — there is no local build step.

### Repo structure

```
vst_playground/
├── JUCE/                   ← shared git submodule (all plugins reference ../JUCE)
├── <plugin_name>/
│   ├── Source/
│   │   ├── PluginProcessor.h/.cpp
│   │   └── PluginEditor.h/.cpp
│   ├── fonts/
│   ├── CMakeLists.txt
│   └── logo_transparent.svg
└── .github/workflows/build.yml
```

Current plugins: `reverse_reverb`, `pitch_wobble`, `through_the_wall`, `drum_smash`, `break_scientist`, `choir_box`, `lizard_suite`.

### Triggering a build

Builds are triggered by pushing to `main` with a [Conventional Commits](https://www.conventionalcommits.org/) scope that matches the plugin folder name:

```bash
git commit -m "feat(drum_smash): add new preset"      # builds drum_smash
git commit -m "fix(pitch_wobble): fix LFO at low rates" # builds pitch_wobble
git commit -m "chore: update readme"                    # no build triggered
```

Manual trigger: **Actions → Build VST → Run workflow** — pick plugin and platform.

### Adding a new plugin

1. Create `<plugin_name>/Source/PluginProcessor.{h,cpp}` and `PluginEditor.{h,cpp}`.
2. Add `<plugin_name>/CMakeLists.txt` — reference JUCE as `../JUCE`:
   ```cmake
   add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../JUCE JUCE_build)
   ```
3. Add `<plugin_name>` to the `options:` list in `.github/workflows/build.yml`.
4. Commit with a matching scope to trigger the first build.

### CMakeLists.txt conventions

- `cmake_minimum_required(VERSION 3.15)`, `CMAKE_CXX_STANDARD 17`
- Use `juce_add_plugin` with `FORMATS VST3` only
- Link: `juce::juce_audio_utils`, `juce::juce_dsp`, plus the three recommended flag libraries
- Binary assets (fonts, SVG) go in a `juce_add_binary_data` target

### C++ conventions

- **Never** use `#include <JuceHeader.h>` — include JUCE modules directly:
  ```cpp
  #include <juce_audio_processors/juce_audio_processors.h>
  #include <juce_dsp/juce_dsp.h>
  ```
- Parameters managed via `juce::AudioProcessorValueTreeState` (`apvts`)
- All processors: `JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR` at the end of the class

### JUCE-free DSP cores

For new DSP plugins, put the signal processing in header-only, JUCE-free classes under `<plugin_name>/Source/DSP/` (no JUCE includes, no heap allocation in `process()`), and keep `PluginProcessor` to parameter plumbing. Add standalone tests in `<plugin_name>/test/test_*.cpp` that build with plain g++, using forwarding headers in `test/` so no include flags are needed:

```bash
cd <plugin_name>/test && g++ -std=c++17 -O2 -Wall test_<name>.cpp -o t && ./t
```

**Run Tests** compiles and runs every `*/test/test_*.cpp` this way before the Catch2 suite. `lizard_suite` is the reference layout.

### Known gotchas

- **macOS bundle corruption**: `upload-artifact` destroys `.vst3` bundle structure. Always zip before uploading (`zip -r Plugin.vst3.zip Plugin.vst3`), then unzip locally before installing.
- **Windows quarantine**: not an issue. macOS requires `sudo xattr -cr` after install.
- **No local build environment**: don't suggest `cmake -B build` locally unless the user explicitly has a dev machine set up. The canonical build path is GitHub Actions.

## Test pages for DSP plugins

Whenever a plugin here is a signal-processing core a person can hear (effects, synths, filters, generative audio), also build and publish a **live browser test page** as an Artifact, without waiting to be asked. The page lets the user try the real algorithm without a DAW or a CI build. Unless the user asks, don't commit the page to the repo. Give the link in the final reply and add it to the PR description.

Reference implementations (read them with the Artifact tool's `read` action and reuse their structure):
- Lizard Dust bench (one core): https://claude.ai/artifact/3eHT84HfhaZaUscjYGSyzq
- Lizard Suite bench (four cores in a chain, plus a level-over-time plot): https://claude.ai/artifact/DRX17NWxVYttefP2P42cd5

### 1. Port the core faithfully, then prove it

- Port the native core (`Source/DSP/*.h`) **line for line** into a plain JS class with the same method names and setters. Don't rewrite or "improve" the algorithm; the page must run the same maths as the code under review. If the processor adds wiring (chain order, per-channel seeds, when setters run), port that too.
- Mirror the native numeric types. For C++ `float` code, wrap every intermediate in `Math.fround`, keep `double` state as JS numbers, and pre-round float literals (`0.6f` is `Math.fround(0.6)`, not `0.6`). Match rounding: `std::round` is half away from zero, and `Math.round` only agrees for non-negative inputs. Plugin parameters arrive as `std::atomic<float>`, so round them to float before calling the setters.
- **Parity-check before building the page:**
  - Compile a small native harness (the cores are JUCE-free, so plain g++ works) that writes a deterministic input and the core's output for 4–5 parameter sets per core, covering every branch (every optional stage on and off, extreme settings, partial mix, more than one host rate).
  - For multi-module plugins, also run the real `PluginProcessor` in a small JUCE console app. Dump the exact parameter floats it used, and compare the whole chain, including a parameter change mid-stream.
  - Process in the same block size the page will use (128), and compare in Node sample by sample.
  - Target 0 mismatches. If any remain, fix the port. Don't ship a page that "roughly" matches.
- Keep the core in its own `dsp.js` file with no backticks or `${`. Inline it into the page with a build step (template placeholder + a Python replace), so one source string feeds both the AudioWorklet and the main thread.

### 2. What the page must include

- **Built-in test sources, generated in code** (no network fetches) that exercise the effect's weak spots:
  - A musical source: a synthesized drum loop with kick, snare, hats and a keys chord with bright upper partials, from a seeded PRNG, normalised to about −1.5 dBFS.
  - An analytic source: a 20 Hz–20 kHz exponential sine sweep.
  - Silence, when the effect generates sound of its own (noise beds, reverb tails).
  - Loading the user's own file, via a button and drag-and-drop (decode with `decodeAudioData`). Microphone access is blocked in artifacts, so use uploads instead.
- **Transport:** a Play/Stop button (audio may only start after a click) and a **Bypass** toggle for A/B comparison.
- **Every parameter the plugin exposes**, with the **same ranges, defaults and skew curves** (replicate JUCE `setSkewForCentre` / skew mappings). Each gets a live readout in real units (kHz, bits, dB, ×, %) and an `aria-valuetext`.
- **Presets** ("starting points") for the characteristic settings from the spec, plus a clean setting. Highlight the preset that matches the current parameters.
- **Derived stats** computed from the parameters, so the user sees why the sound changes (e.g. hold length, reduced Nyquist, quantiser steps, comb feedback and RT60, dropout rate, noise-bed levels).
- **Visualisations of input vs output:**
  - A log-frequency spectrum with a dB grid. Draw the input as a muted line and the output as an accent line with a filled area. Mark the key frequencies and shade the region where the artifacts live.
  - A sample-level waveform zoom (about 160 samples) that draws the output as steps.
  - A level-over-time plot (RMS in 10 ms windows over a few seconds) when the effect acts over time (dropouts, tails, envelopes).
  - Use your own FFT (radix-2, Hann window, full-scale sine = 0 dB) on analyser *time-domain* data, so the live view and the static view use the same scale.
- **A static preview at rest:** before Play, run the same JS core offline on a representative slice of the current source and draw the plots. Re-render on every parameter change. The page must be informative before any sound plays.
- **"What to listen for" cards**, one per acceptance criterion in the spec. Each explains what to hear or see, and has a one-click button that sets the relevant parameters and starts playback.
- **A footer** saying the page runs a JS port of the named source files (not the compiled plugin), what the parity check showed, and any behaviour matched on purpose (e.g. params applied per 128-sample block, unsmoothed). Link to the PR.

### 3. Audio plumbing

- Run the core in an **AudioWorklet** loaded from a `blob:` URL (the DSP source plus the processor class). Use one core instance per channel, the host rate from the global `sampleRate`, and send parameters via `port.postMessage`, applied once per block like the plugin.
- Fall back to a `ScriptProcessorNode` running the same class if `addModule` fails.
- Signal chain: source → pre-analyser, and source → effect → post-analyser → gain (≈0.9) → destination. Reset the DSP state when (re)starting.
- Create the `AudioContext` on load (it starts suspended) so the real host rate is known for the preview and readouts, and `resume()` it on Play.

### 4. Artifact rules that bite here

- Follow the Artifact tool's quickstart and design guidance. Build a single self-contained HTML file with a `<title>` of 2–4 words (the plugin name), light and dark tokens, phone-width layout, and a visible focus state.
- Scripts may load only from the allowed CDNs, and fonts only from Google Fonts. Everything else must be inline. No `fetch` of external audio, no downloads, no `alert`.
- Give the page a visual identity that fits the subject (e.g. hardware-panel labels and LCD readouts), not a generic dashboard. Keep the Lizard pages visually consistent with each other.

### 5. Check once, publish, report

- Take one headless-Chromium screenshot of the built file (`executablePath: '/opt/pw-browsers/chromium'`) to catch layout or script errors, fix what it shows, then publish. The Google Fonts certificate error is expected in the sandbox.
- In the reply, say:
  - what the page lets the user test, including the source types and which acceptance criteria are one click away;
  - that it's a verified port, not the compiled binary;
  - what still needs native testing (the CI builds on macOS and Windows, DAW listening).
