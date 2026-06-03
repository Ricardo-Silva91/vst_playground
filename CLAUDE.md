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

Current plugins: `reverse_reverb`, `pitch_wobble`, `through_the_wall`, `drum_smash`, `break_scientist`, `choir_box`.

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

### Known gotchas

- **macOS bundle corruption**: `upload-artifact` destroys `.vst3` bundle structure. Always zip before uploading (`zip -r Plugin.vst3.zip Plugin.vst3`), then unzip locally before installing.
- **Windows quarantine**: not an issue. macOS requires `sudo xattr -cr` after install.
- **No local build environment**: don't suggest `cmake -B build` locally unless the user explicitly has a dev machine set up. The canonical build path is GitHub Actions.
