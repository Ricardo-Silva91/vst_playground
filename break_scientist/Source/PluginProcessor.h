#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

// ── Preset struct ─────────────────────────────────────────────────────────────
struct BreakPreset
{
    const char* name;
    float swing;          // 0.5–0.75  (50%=straight, 75%=heavy shuffle)
    float humanize;       // 0–1       (random micro-offset depth)
    float drag;           // 0–1       (consistent pull-back on all hits)
    float sensitivity;    // 0–1       (transient detection threshold)
    float velocityVar;    // 0–1       (hit amplitude reshaping)
    float wetMix;         // 0–1
};

static const BreakPreset kPresets[] =
{
    // name            swing   hum    drag   sens   vel    wet
    { "Donuts",        0.68f,  0.75f, 0.65f, 0.55f, 0.60f, 1.0f  },  // Heavy Dilla
    { "MPC Straight",  0.58f,  0.15f, 0.10f, 0.50f, 0.25f, 1.0f  },  // Clean boom bap
    { "Drunk Shuffle", 0.62f,  0.95f, 0.30f, 0.45f, 0.50f, 1.0f  },  // Falling-apart feel
    { "Jungle Step",   0.55f,  0.10f, 0.05f, 0.60f, 0.15f, 1.0f  },  // Tight Amen territory
    { "Ghost Town",    0.60f,  0.40f, 0.20f, 0.35f, 0.90f, 1.0f  },  // Ghost hits everywhere
    { "Straight Up",   0.50f,  0.05f, 0.00f, 0.50f, 0.05f, 1.0f  },  // Near-bypass
};
static constexpr int kNumPresets = (int)(sizeof(kPresets) / sizeof(kPresets[0]));

// ── Processor ─────────────────────────────────────────────────────────────────
class BreakScientistProcessor : public juce::AudioProcessor
{
public:
    BreakScientistProcessor();
    ~BreakScientistProcessor() override;

    void prepareToPlay  (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock   (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Break Scientist"; }
    bool acceptsMidi()  const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.15; }  // lookahead

    // Report latency so host can compensate
    int getLatencySamples() const { return lookaheadSamples; }

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

    // ── Lookahead buffer ──────────────────────────────────────────────────────
    // We buffer incoming audio by `lookaheadSamples` so we can push hits
    // either forward (earlier) or backward (later) relative to the grid.
    static constexpr int kMaxLookaheadMs = 150;
    int lookaheadSamples = 0;

    // Circular lookahead buffer — stereo
    std::vector<float> lookaheadL;
    std::vector<float> lookaheadR;
    int lookaheadWritePos = 0;

    // ── Transient detection ───────────────────────────────────────────────────
    // Envelope follower to track RMS level; onset detected when instantaneous
    // energy exceeds a multiple of the running RMS.
    float envelopeL      = 0.f;
    float envelopeR      = 0.f;
    float runningRms     = 0.f;
    float rmsSmooth      = 0.f;
    bool  inTransient    = false;
    int   transientCooldown = 0;   // samples to ignore after an onset

    // ── Hit displacement queue ────────────────────────────────────────────────
    // When a transient is detected, we schedule a "hit event" to be played
    // back at a displaced time via a short output buffer.
    struct HitEvent
    {
        int    outputSampleOffset;  // samples from now to play this hit
        float  gainScale;           // velocity variance applied here
        bool   active = false;
    };
    static constexpr int kMaxHits = 32;
    HitEvent hitQueue[kMaxHits];
    int hitQueueCount = 0;

    // Output crossfade buffer — holds the displaced hit being written out
    static constexpr int kHitBufSize = 8192;
    std::vector<float> hitBufL;
    std::vector<float> hitBufR;
    int   hitBufWritePos  = 0;
    int   hitBufReadPos   = 0;
    int   hitBufRemaining = 0;   // samples left to output from current hit
    float crossfadeLen    = 0.f; // in samples, for fade in/out

    // ── Swing / timing state ──────────────────────────────────────────────────
    // 16th-note grid tracking — updated per block from host BPM if available,
    // otherwise estimated from detected transient inter-onset intervals.
    double currentBpm        = 120.0;
    double sixteenthSamples  = 0.0;   // samples per 16th note
    int    gridPhase         = 0;     // sample counter within current 16th
    int    sixteenthCount    = 0;     // which 16th note we're on (0,1,2,3...)

    // Inter-onset interval estimator (fallback when no host BPM)
    int    lastOnsetSample   = 0;
    double ioiEstimate       = 0.0;   // running average inter-onset interval
    int    ioiCount          = 0;

    // ── RNG for humanize ─────────────────────────────────────────────────────
    juce::Random rng;

    int    currentPreset     = 0;
    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BreakScientistProcessor)
};