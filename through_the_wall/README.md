# Through The Wall

A VST3 effect that simulates sound transmitting through a wall — the muffled, bassy, room-bleed character of audio heard from the other side of drywall or concrete.

## What It Does

When sound passes through a wall, several things happen simultaneously:

- High frequencies are heavily absorbed, leaving a low-end-dominated residue
- The wall itself resonates at certain frequencies, adding a hollow, buzzy coloration
- Diffuse room ambience bleeds through, making the source sound spatially distant
- Transients smear and the overall image feels indistinct

**Through The Wall** models all four of these phenomena independently, so you can dial in anything from "one room over" to "three floors up."

## Parameters

| Knob | Range | What It Does |
|---|---|---|
| **Wall Thickness** | 0–1 | Controls a dual cascaded low-pass filter. At 0, minimal filtering (~4kHz cutoff). At 1, only a muddy bass bloom remains (~150Hz). |
| **Room Bleed** | 0–1 | Wet mix of a Schroeder reverb network (4 comb + 2 allpass filters) simulating the diffuse ambience of the room on the other side. |
| **Wall Rattle** | 0–1 | A short comb filter (2–8ms delay) that simulates the wall resonating at certain frequencies — the hollow, ringing buzz drywall adds. |
| **Distance** | 0–1 | Attenuates the dry signal while scaling up reverb contribution, moving the source further away. At max, the dry signal is nearly gone. |

## Signal Chain

```
Input → Wall Rattle (comb) → Room Bleed (reverb mix + Distance scaling) → Wall Thickness (dual LPF) → Output
```

All parameters are smoothed to prevent zipper noise when automating.

## Building

This plugin follows the `vst_playground` monorepo conventions. From the repo root:

```bash
cd through_the_wall
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

JUCE is referenced as a shared submodule at the repo root (`../JUCE`). Requires **JUCE 8.0.4+** (7.x has issues on macOS 15).

### CI / GitHub Actions

Trigger a build by committing with the scope `through_the_wall`:

```
feat(through_the_wall): your message here
```

Or use the manual `workflow_dispatch` dropdown and select `through_the_wall`.

## Installation

**macOS:**
```bash
# Copy the .vst3 bundle to the system plugin folder
cp -r build/through_the_wall_artefacts/Release/VST3/Through\ The\ Wall.vst3 /Library/Audio/Plug-Ins/VST3/

# Remove Gatekeeper quarantine flag
xattr -dr com.apple.quarantine "/Library/Audio/Plug-Ins/VST3/Through The Wall.vst3"
```

**Windows:**

Copy `Through The Wall.vst3` to `C:\Program Files\Common Files\VST3\` and rescan in your DAW.

## Use Cases

- Making a sound feel like it's coming from an adjacent room or apartment
- Layering a heavily processed version under a dry signal for spatial depth
- Sound design for film/game audio — doors, walls, vehicles
- Creating that "heard through the floor" low-end rumble on kicks or bass
- Subtly degrading a signal to make it feel more distant or embedded in a space

## Technical Notes

- **Format:** VST3 only, stereo in/out, no MIDI
- **Tail:** ~2 seconds (reverb decay)
- **Reverb model:** Schroeder network with delay line sizes scaled to sample rate (prime-spaced to minimize modal clustering)
- **Filter topology:** Two cascaded IIR low-pass filters at different cutoff frequencies for a steeper, more realistic wall absorption curve
- **No external dependencies** beyond JUCE DSP module
