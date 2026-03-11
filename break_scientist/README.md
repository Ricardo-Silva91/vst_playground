# Drum Smash

A drum effects processor VST3 with 8 hand-crafted presets, each applying a different character transformation to your drum bus or individual drum tracks.

---

## Presets

| # | Name | Description |
|---|------|-------------|
| 0 | **Dusty Vinyl** | Vinyl crackle + noise, warm LPF roll-off, gentle wow/flutter, light compression. Makes drums sound like they're coming off an old 7" single. |
| 1 | **Heavy Hitter** | Heavy compression with slow release, hard drive saturation, transient boost, and wide stereo. Huge, punchy stadium drums. |
| 2 | **Bit Crusher** | 4-bit depth + 8x sample-rate division. Gritty, lo-fi digital destruction. Snares snap, kicks clank. |
| 3 | **Lo-Fi Radio** | Narrow bandwidth (HPF 300 Hz / LPF 3.5 kHz), 10-bit depth, moderate wow flutter, reduced stereo width. Sounds like a drummer on AM radio. |
| 4 | **Punchy Club** | Wide open filters, moderate drive, heavy compression with fast attack, wide stereo reverb. Dance-floor ready. |
| 5 | **Tape Warped** | Slow wow/flutter (2.5 Hz), 14-bit warmth, tape saturation drive, gentle LPF, medium room reverb. Worn-out cassette aesthetic. |
| 6 | **Nuclear Snare** | Extreme compression (15:1 ratio), full saturation, fast attack, huge transient boost, wide reverb tail. Every hit sounds like an explosion. |
| 7 | **Bedroom Boom-Bap** | Vinyl noise, 12-bit, 2x sample-rate reduction, slight pitch-down, subtle wow, tight compression. Classic 90s hip-hop sampler sound. |

---

## Parameters

### Crusher
- **Bit Depth** — 1–16 bits. Lower = more digital grit
- **SR Divide** — 1–32× sample-rate downsampling

### Saturation
- **Drive** — Soft-clip saturation (0–1)
- **Output Gain** — Post-processing level (0–2×)

### Character
- **Noise** — Vinyl hiss amount
- **Crackle** — Random crackle event density

### Filters
- **LPF Cutoff** — Low-pass cutoff (200–22000 Hz)
- **HPF Cutoff** — High-pass cutoff (20–2000 Hz)

### Compressor
- **Threshold** — dBFS (-60 to 0)
- **Ratio** — 1:1 to 20:1
- **Attack** — 0.1–200 ms
- **Release** — 10–2000 ms
- **Makeup** — 0–24 dB makeup gain

### Reverb
- **Room** — Room size (0–1)
- **Rev Wet** — Wet mix (0–1)
- **Damping** — High-frequency damping (0–1)

### Modulation
- **Pitch** — Pitch shift in semitones (-12 to +12)
- **Wow Rate** — Wow/flutter LFO rate (0–8 Hz)
- **Wow Depth** — Modulation depth in cents (0–50)

### Spatial
- **Width** — Mid/side stereo width (0 = mono, 2 = extra-wide)
- **Transient** — Transient shaper boost on attack (0–1)

---

## Signal Chain

```
Input → Pitch/Wow Modulation → Bit Crusher → Drive Saturation
      → Vinyl Noise/Crackle → Transient Shaper → HPF → LPF
      → Compressor (+ Makeup) → Reverb → Stereo Width → Output Gain
```

---

## Build

```bash
cd drum_smash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

### macOS install
```bash
cd build/drum_smash_artefacts/Release/VST3/
ZIP_NAME=$(basename "Drum Smash.vst3" | tr ' ' '_').zip
zip -r "$ZIP_NAME" "Drum Smash.vst3"
# unzip to /Library/Audio/Plug-Ins/VST3/
sudo xattr -dr com.apple.quarantine "/Library/Audio/Plug-Ins/VST3/Drum Smash.vst3"
```

### Windows install
Copy `Drum Smash.vst3` to `C:\Program Files\Common Files\VST3\`

---

## Use Cases
- Hip-hop/trap drum bus processing
- Lo-fi beat production
- Rock / metal snare enhancement
- Vintage sample aesthetics
- Experimental sound design
