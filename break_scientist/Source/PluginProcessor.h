#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <array>

// ── Preset struct ─────────────────────────────────────────────────────────────
struct BreakPreset
{
    const char* name;
    float swing;          // 0.5–0.75  (50%=straight, 75%=heavy shuffle)
    float humanize;       // 0–1       (random micro-offset depth)
    float drag;           // 0–1       (consistent pull-back on all hits, maps to 0–80ms)
    float sensitivity;    // 0–1       (transient detection threshold)
    float velocityVar;    // 0–1       (hit amplitude reshaping)
    float wetMix;         // 0–1
};

static const BreakPreset kPresets[] =
{
    // name              swing   hum    drag   sens   vel    wet
    // ── Classic ───────────────────────────────────────────────────────────────
    { "Donuts",          0.68f,  0.75f, 0.65f, 0.55f, 0.60f, 1.0f  },  // Heavy Dilla pocket
    { "MPC Straight",    0.58f,  0.15f, 0.10f, 0.50f, 0.25f, 1.0f  },  // Clean boom bap
    { "Drunk Shuffle",   0.62f,  0.95f, 0.30f, 0.45f, 0.50f, 1.0f  },  // Falling-apart feel
    { "Jungle Step",     0.55f,  0.10f, 0.05f, 0.60f, 0.15f, 1.0f  },  // Tight Amen territory
    { "Ghost Town",      0.60f,  0.40f, 0.20f, 0.35f, 0.90f, 1.0f  },  // Ghost hits everywhere
    { "Straight Up",     0.50f,  0.05f, 0.00f, 0.50f, 0.05f, 1.0f  },  // Near-bypass
    // ── Aggressive ────────────────────────────────────────────────────────────
    { "Off The Grid",    0.80f,  1.00f, 0.90f, 0.50f, 0.70f, 1.0f  },  // Extreme swing+drag+chaos
    { "Wrong Tempo",     0.75f,  0.85f, 1.00f, 0.45f, 0.40f, 1.0f  },  // Maximum drag, feels broken
    { "Slipping Away",   0.70f,  1.00f, 0.50f, 0.40f, 0.80f, 1.0f  },  // High humanize dominates
    { "Trap Drunk",      0.82f,  0.60f, 0.75f, 0.55f, 0.55f, 1.0f  },  // Heavy swing, moderate drag
};
static constexpr int kNumPresets = (int)(sizeof(kPresets) / sizeof(kPresets[0]));

// ── Processor ─────────────────────────────────────────────────────────────────
class BreakScientistProcessor : public juce::AudioProcessor
{
public:
    BreakScientistProcessor();
    ~BreakScientistProcessor() override;

    void prepareToPlay   (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock    (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName()    const override { return "Break Scientist"; }
    bool acceptsMidi()              const override { return false; }
    bool producesMidi()             const override { return false; }
    double getTailLengthSeconds()   const override { return 1.0; }

    int  getNumPrograms()    override { return kNumPresets; }
    int  getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram  (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName  (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    int getCurrentPresetIndex() const { return currentPreset; }

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void applyPreset (int index);

    // ── Architecture overview ─────────────────────────────────────────────────
    //
    // We use a large pre-roll (input) ring buffer — 1 second at the current
    // sample rate. All incoming audio is written into it continuously.
    //
    // The output is assembled into a matching output ring buffer, initialised
    // to silence. When a transient is detected we:
    //   1. Note the onset position in the input ring buffer.
    //   2. Calculate a total displacement (drag + swing + humanize).
    //   3. Copy a window of audio (~120ms) from the input ring buffer,
    //      scaled by the velocity gain, into the output ring buffer at
    //      (onset_position + displacement), using a Hann-window envelope
    //      to ensure clean fade-in and fade-out on every hit.
    //   4. Mark those output positions in ringOutMask so the original
    //      pass-through audio is suppressed there (avoiding doubling).
    //
    // The output ring buffer read position always lags the write position by
    // exactly `lookaheadSamples`. This is reported to the host as latency so
    // the DAW compensates automatically during playback.
    // After consolidating/freezing the track the latency offset disappears.
    //
    // ─────────────────────────────────────────────────────────────────────────

    static constexpr int kMaxDisplaceMs = 500;  // max total displacement budget

    int    ringSize         = 0;   // set in prepareToPlay (kRingSeconds * sr)
    int    lookaheadSamples = 0;   // reported latency = kMaxDisplaceMs converted

    // Input ring — raw incoming audio, written every block
    std::vector<float> ringInL, ringInR;
    int ringWritePos = 0;

    // Output ring — pre-filled with silence; hit copies are added here
    // at displaced positions. Pass-through fills any unmasked positions.
    std::vector<float> ringOutL, ringOutR;

    // Suppression mask: value in [0,1]. 1 = fully suppress the original
    // pass-through signal at this position (a displaced hit is there instead).
    std::vector<float> ringOutMask;

    int ringReadPos = 0;   // lags ringWritePos by lookaheadSamples

    // ── Transient detection ───────────────────────────────────────────────────
    float envelope        = 0.f;
    float rmsSmooth       = 0.f;
    float runningRms      = 0.f;
    bool  inTransient     = false;
    int   cooldownSamples = 0;

    // ── Swing / grid ──────────────────────────────────────────────────────────
    double currentBpm        = 120.0;
    double sixteenthSamples  = 0.0;
    int    sixteenthCount    = 0;
    int    gridPhase         = 0;

    // IOI-based BPM fallback
    int    lastOnsetRingPos  = -1;
    double ioiEstimate       = 0.0;
    int    ioiCount          = 0;

    // ── RNG ───────────────────────────────────────────────────────────────────
    juce::Random rng;

    int    currentPreset     = 0;
    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BreakScientistProcessor)
};