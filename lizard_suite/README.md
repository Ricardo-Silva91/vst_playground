# Lizard Suite

Four lo-fi modules in one plugin, run in this order on each channel:

    Dust (sampler) → Chew (tape) → Murk (room) → Vinyl (record)

Each module is a header-only DSP core in `Source/DSP/` with no JUCE dependency and no heap allocation in `process()`, so it can be tested with plain g++. Lizard Dust comes from `claude-sandbox/lizard-dust`; Vinyl, Murk and Chew are new.

- **Dust**: sample-&-hold rate reduction with no reconstruction filter, a bit-depth crush with no dither, and an optional 1-pole low-pass with tanh drive.
- **Chew**: worn-tape dropouts. Random level sags recover toward unity through a smoothing filter. It only applies gain, so |out| ≤ |in|.
- **Murk**: a dark Schroeder/Freeverb-style reverb (4 damped combs into 2 allpasses) for a basement or underwater space. Its delay lines are allocated in `setHostSampleRate()`, which the plugin calls from `prepareToPlay()`.
- **Vinyl**: a procedural noise bed with age-controlled hiss, ~50 Hz turntable rumble and sparse decaying crackle. It adds noise on top of the signal.

## Parameters

| Module | Param | Range | Default | Notes |
|---|---|---|---|---|
| Dust | Rate | 1 000–48 000 Hz | 26 040 | Skewed around 12 kHz. At or above the host rate it has no effect. |
| Dust | Bits | 2–16 | 12 | |
| Dust | Low-pass | 0–20 000 Hz | 0 (Off) | Skewed around 4 kHz. |
| Dust | Drive | 1–8 | 1 | |
| Dust | Mix | 0–1 | 1 | **0 = off**, bit-identical dry. |
| Chew | Rate | 0–20 /s | 3 | Average dropouts per second. Skewed around 4. |
| Chew | Depth | 0–1 | 0.5 | Deepest sag. **0 = off**, bit-identical dry. |
| Chew | Smooth | 1–50 ms | 8 | Gain smoothing time. Skewed around 10 ms. |
| Murk | Room | 0–1 | 0.6 | Comb feedback 0.70–0.98. Stable at max. |
| Murk | Damp | 0–1 | 0.5 | Low-pass in the comb feedback. Higher is darker. |
| Murk | Mix | 0–1 | 0.3 | **0 = off**, bit-identical dry. |
| Vinyl | Age | 0–1 | 0.4 | Older gives a louder bed and denser pops. |
| Vinyl | Amount | 0–1 | 0.5 | **0 = off**, bit-identical dry. |
| Vinyl | Seed | 1–999 | 1 | Picks the noise pattern. |

Parameters are applied once per block with no smoothing, so each "off" setting stays an exact dry path. Chew uses the same seed on both channels, so dropouts hit left and right together like a real tape. Vinyl gets a different seed per channel, so the hiss is stereo. When Seed changes, the noise stream is re-seeded once, not on every block.

## Layout

- `Source/DSP/Lizard{Dust,Chew,Murk,Vinyl}.h`: the JUCE-free cores.
- `Source/PluginProcessor.*`: one instance of each core per channel (mono or stereo).
- `Source/PluginEditor.*`: custom editor, one strip per module in signal order.
- `test/`: standalone core tests. The forwarding headers let them build from this folder with no include flags.

## Tests

Core tests (no JUCE). CI runs these in **Run Tests**:

    cd test && g++ -std=c++17 -O2 -Wall test_lizardsuite.cpp -o t && ./t
    cd test && g++ -std=c++17 -O2 -Wall test_lizarddust.cpp  -o t && ./t

Plugin tests (Catch2): `tests/lizard_suite_test.cpp`. They check the off settings are bit-identical dry, Murk stays stable at max Room, Chew is pure gain and linked across channels, and the Vinyl seed behaves.
