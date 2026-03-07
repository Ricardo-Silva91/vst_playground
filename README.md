# vst_playground

A collection of VST3 plugins built with [JUCE](https://juce.com/) and CMake, compiled in the cloud via GitHub Actions. No local Xcode or Visual Studio required.

---

## Repo Structure

```
vst_playground/
├── JUCE/                        ← JUCE as a git submodule (shared by all plugins)
├── reverse_reverb/
│   ├── Source/
│   │   ├── PluginProcessor.h/.cpp
│   │   └── PluginEditor.h/.cpp
│   └── CMakeLists.txt
├── pitch_wobble/
│   ├── Source/
│   │   ├── PluginProcessor.h/.cpp
│   │   └── PluginEditor.h/.cpp
│   └── CMakeLists.txt
└── .github/
    └── workflows/
        └── build.yml            ← single shared workflow for all plugins
```

Each plugin is a self-contained folder with its own `CMakeLists.txt`. JUCE lives once at the repo root as a submodule and is referenced by all plugins via a relative path (`../JUCE`).

---

## JUCE Submodule

JUCE is included as a git submodule rather than being downloaded per-plugin via `FetchContent`. This keeps builds faster and avoids redundant clones.

### First-time clone

```bash
git clone --recurse-submodules https://github.com/yourname/vst_playground.git
```

### If you already cloned without submodules

```bash
git submodule update --init --recursive
```

### Updating JUCE

```bash
cd JUCE
git checkout 7.0.9   # or whichever tag you want
cd ..
git add JUCE
git commit -m "chore: bump JUCE to 7.0.9"
```

### How each plugin's CMakeLists.txt references it

Instead of `FetchContent`, each plugin uses:

```cmake
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../JUCE JUCE_build)
```

This means all plugins share the same JUCE source. GitHub Actions checks out the submodule automatically (see workflow notes below).

---

## Building in the Cloud (GitHub Actions)

Builds are triggered in two ways:

### 1. Commit scope (automatic)

The workflow reads the scope from your commit message using [Conventional Commits](https://www.conventionalcommits.org/) format:

```
type(scope): description
```

The scope must exactly match the plugin's folder name:

```bash
git commit -m "feat(pitch_wobble): add smoothness knob"     # builds pitch_wobble
git commit -m "fix(reverse_reverb): fix wet mix at 100%"    # builds reverse_reverb
git commit -m "chore: update readme"                         # no build triggered
```

If no scope is present, no build runs.

### 2. Manual trigger (workflow_dispatch)

Go to **Actions → Build VST → Run workflow** on GitHub. You'll get a dropdown to pick which plugin to build.

---

## Installing a Built Plugin

After a build completes, download the artifact from the GitHub Actions run page.

### macOS

1. Unzip the downloaded `.vst3.zip` — this preserves the macOS bundle structure
2. Copy the `.vst3` bundle to:
   ```
   /Library/Audio/Plug-Ins/VST3/
   ```
3. Remove the quarantine flag (required to bypass Gatekeeper):
   ```bash
   xattr -dr com.apple.quarantine /Library/Audio/Plug-Ins/VST3/YourPlugin.vst3
   ```
4. Rescan plugins in your DAW

### Windows

1. Unzip the artifact
2. Copy the `.vst3` folder to:
   ```
   C:\Program Files\Common Files\VST3\
   ```
3. Rescan plugins in your DAW (in FL Studio: Options → Manage plugins → Find more plugins)

---

## Known Issues & Fixes

### `#include <JuceHeader.h>` causes build failure

Projucer-style includes don't work with CMake. Every source file must include JUCE modules directly:

```cpp
// ❌ Don't use this
#include <JuceHeader.h>

// ✅ Use direct module includes instead
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
```

### macOS bundle structure gets corrupted by the artifact zip

GitHub Actions' `upload-artifact` re-zips files and destroys the `.vst3` bundle's internal directory structure. The fix is to manually zip the bundle *before* uploading:

```yaml
- name: Zip VST3 bundle
  run: |
    cd build
    zip -r MyPlugin.vst3.zip MyPlugin.vst3
- uses: actions/upload-artifact@v4
  with:
    path: build/MyPlugin.vst3.zip   # upload the zip, not the raw bundle
```

Then unzip locally before installing (see Installing section above).

---

## Adding a New Plugin

1. Create a new folder at the repo root (e.g. `my_plugin/`)
2. Add `Source/PluginProcessor.h`, `Source/PluginProcessor.cpp`, `Source/PluginEditor.h`, `Source/PluginEditor.cpp`
3. Add a `CMakeLists.txt` — reference JUCE as `../JUCE`:
   ```cmake
   add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../JUCE JUCE_build)
   ```
4. Add the folder name to the `options:` list in `.github/workflows/build.yml`:
   ```yaml
   options:
     - reverse_reverb
     - pitch_wobble
     - my_plugin      ← add this
   ```
5. Commit with the matching scope to trigger a build:
   ```bash
   git commit -m "feat(my_plugin): initial version"
   ```

---

## Plugins

| Plugin | Description | Knobs |
|---|---|---|
| `reverse_reverb` | Reverses audio in a window and applies reverb for a swell effect | Room Size, Wet Mix, Window (ms) |
| `pitch_wobble` | Applies subtle random pitch deviations for an organic, human feel | Depth (cents), Rate (Hz), Smoothness |
