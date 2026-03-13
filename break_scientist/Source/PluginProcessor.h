#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>

// ── Preset struct ─────────────────────────────────────────────────────────────
struct BreakPreset
{
    const char* name;
    float swing;          // 0.50–0.85
    float humanize;       // 0–1  (maps to ±80ms)
    float drag;           // 0–1  (maps to 0–200ms)
    float sensitivity;    // 0–1
    float velocityVar;    // 0–1
    float wetMix;         // 0–1
};

static const BreakPreset kPresets[] =
{
    // name              swing   hum    drag   sens   vel    wet
    // ── Classic ───────────────────────────────────────────────────────────────
    { "Donuts",          0.68f,  0.75f, 0.65f, 0.55f, 0.60f, 1.0f  },
    { "MPC Straight",    0.58f,  0.15f, 0.10f, 0.50f, 0.25f, 1.0f  },
    { "Drunk Shuffle",   0.62f,  0.95f, 0.30f, 0.45f, 0.50f, 1.0f  },
    { "Jungle Step",     0.55f,  0.10f, 0.05f, 0.60f, 0.15f, 1.0f  },
    { "Ghost Town",      0.60f,  0.40f, 0.20f, 0.35f, 0.90f, 1.0f  },
    { "Straight Up",     0.50f,  0.05f, 0.00f, 0.50f, 0.05f, 1.0f  },
    // ── Aggressive ────────────────────────────────────────────────────────────
    { "Off The Grid",    0.80f,  1.00f, 0.90f, 0.50f, 0.70f, 1.0f  },
    { "Wrong Tempo",     0.75f,  0.85f, 1.00f, 0.45f, 0.40f, 1.0f  },
    { "Slipping Away",   0.70f,  1.00f, 0.50f, 0.40f, 0.80f, 1.0f  },
    { "Trap Drunk",      0.82f,  0.60f, 0.75f, 0.55f, 0.55f, 1.0f  },
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
    double getTailLengthSeconds()   const override { return 2.0; }

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

    // ── Architecture ──────────────────────────────────────────────────────────
    //
    // Latency = 2 seconds. The read pointer lags the write pointer by exactly
    // this amount. By the time we output a sample, we have 2 full seconds of
    // future audio already sitting in the ring buffer.
    //
    // Onset detection runs on the WRITE side (incoming audio), 2 seconds ahead
    // of what we're currently outputting. When we detect an onset at write
    // position W, we:
    //
    //   1. Scan forward from W to find the true peak of the hit (up to 20ms)
    //      — this anchors the Hann window at the actual attack, not the rise
    //   2. Calculate displacement (drag + swing + humanize)
    //   3. Write a Hann-windowed copy of the hit into the output ring at
    //      (W - lookaheadSamples + displacement), i.e. at the output-time
    //      position corresponding to W, shifted by the displacement
    //   4. Write a FULL suppression mask (1.0) immediately at the onset in
    //      the output ring — not Hann-ramped, so the original is killed
    //      instantly with no bleed. Only the OUTPUT copy uses Hann.
    //
    // Undetected hits (hi-hats, quiet transients below threshold) pass through
    // the output ring untouched at their original positions.
    //
    // ─────────────────────────────────────────────────────────────────────────

    // 2 second lookahead
    static constexpr int    kLookaheadMs   = 2000;
    // Max displacement budget (must be < kLookaheadMs)
    static constexpr int    kMaxDisplaceMs = 500;
    // Hit window copied per onset (~150ms)
    static constexpr double kHitWindowSec  = 0.150;
    // How far forward to scan for the true peak after onset (20ms)
    static constexpr double kPeakScanSec   = 0.020;
    int ringSize         = 0;
    int lookaheadSamples = 0;

    std::vector<float> ringInL,   ringInR;    // raw input
    std::vector<float> ringOutL,  ringOutR;   // assembled output (displaced hits)
    std::vector<float> ringMask;              // suppression mask (0=passthru, 1=suppress)

    int ringWritePos = 0;   // advances with incoming audio
    int ringReadPos  = 0;   // lags ringWritePos by lookaheadSamples

    // ── Onset detection (runs at write position) ──────────────────────────────
    // Peak-picking on a smoothed spectral flux / energy derivative.
    // We track a fast envelope and a slow RMS; onset fires when the fast
    // envelope exceeds (slow RMS × threshold).
    float envelope        = 0.f;
    float rmsSmooth       = 0.f;
    float runningRms      = 0.f;
    bool  inTransient     = false;
    int   cooldownSamples = 0;

    // ── Grid / swing ──────────────────────────────────────────────────────────
    double currentBpm       = 120.0;
    double sixteenthSamples = 0.0;
    int    sixteenthCount   = 0;
    int    gridPhase        = 0;

    // IOI fallback
    int    lastOnsetRingPos = -1;
    double ioiEstimate      = 0.0;
    int    ioiCount         = 0;

    juce::Random rng;

    int    currentPreset     = 0;
    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BreakScientistProcessor)
};