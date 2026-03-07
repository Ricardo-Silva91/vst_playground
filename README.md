# ReverseReverb VST3

A VST3 audio effect that captures, isolates, and reverses the reverb tail of incoming audio.

---

## First-time setup (do this once)

### 1. Install Git
Download from https://git-scm.com if you don't have it.

### 2. Create a GitHub repo
- Go to github.com → New repository → name it `vst_playground`
- Set it to Private or Public (your choice)

### 3. Clone your repo and add these files
```bash
git clone https://github.com/YOUR_USERNAME/vst_playground.git
cd vst_playground
```
Copy all the project files into this folder.

### 4. Add JUCE as a submodule (one time only)
```bash
git submodule add https://github.com/juce-framework/JUCE.git JUCE
git submodule update --init --recursive
```
This downloads JUCE into a `JUCE/` folder inside your project.
You do NOT need to install Xcode or Visual Studio locally.

### 5. Push to GitHub
```bash
git add .
git commit -m "Initial commit"
git push origin main
```

---

## Every time you make changes

```bash
git add .
git commit -m "describe what you changed"
git push origin main
```

That's it. GitHub Actions automatically starts building.

---

## Downloading your compiled VST3

1. Go to your repo on github.com
2. Click the **Actions** tab
3. Click the latest workflow run
4. Scroll to the bottom → **Artifacts**
5. Download `ReverseReverb-VST3-Windows` or `ReverseReverb-VST3-Mac`
6. Unzip it → you get `ReverseReverb.vst3`

---

## Installing in FL Studio (Windows)

1. Copy `ReverseReverb.vst3` to:
   ```
   C:\Program Files\Common Files\VST3\
   ```
2. Open FL Studio → Options → Manage Plugins → Start Scan
3. The plugin will appear in your plugin browser under Effects

---

## Installing in FL Studio (Mac)

1. Download the `ReverseReverb-VST3-Mac` artifact from GitHub Actions — this gives you `ReverseReverb-Mac.zip`
2. Unzip it — you'll get a `ReverseReverb.vst3` folder (not just a `Contents/` folder)
3. Copy `ReverseReverb.vst3` to:
   ```
   /Library/Audio/Plug-Ins/VST3/
   ```
4. Run this command in Terminal to remove Apple's quarantine flag:
   ```bash
   xattr -dr com.apple.quarantine /Library/Audio/Plug-Ins/VST3/ReverseReverb.vst3
   ```
5. Restart FL Studio and scan for new plugins

> **Why the Terminal command?** macOS Gatekeeper blocks plugins that aren't signed with a paid Apple Developer certificate ($99/year). The `xattr` command manually approves the plugin. You'll need to run it once every time you install a newly built version.

---

## Project structure

```
ReverseReverb/
├── .github/
│   └── workflows/
│       └── build.yml        ← GitHub Actions build config
├── JUCE/                    ← JUCE framework (git submodule, auto-downloaded)
├── CMakeLists.txt           ← build system config
├── PluginProcessor.h        ← audio engine declarations
├── PluginProcessor.cpp      ← audio engine logic (reverb, reversing)
├── PluginEditor.h           ← UI declarations
├── PluginEditor.cpp         ← UI layout and knobs
└── README.md
```

---

## Knobs

| Knob | Range | Description |
|------|-------|-------------|
| Room Size | 0.1 – 1.0 | Size of the reverb before it's reversed |
| Wet Mix | 0.0 – 1.0 | 0 = dry only, 1 = reversed reverb only |
| Window (ms) | 100 – 2000ms | Capture window length — also the latency |