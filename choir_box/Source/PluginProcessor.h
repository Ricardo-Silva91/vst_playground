#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>
#include <complex>

// ── PitchShifter — correct phase vocoder ─────────────────────────────────────
// Uses JUCE's FFT with proper in-place real-only packing/unpacking.
// Processes audio in hop-sized blocks with 4x overlap.
// Latency = kFftSize samples (~46ms @44.1k), reported to host.
class PitchShifter
{
public:
    static constexpr int kFftOrder  = 11;           // 2^11 = 2048
    static constexpr int kFftSize   = 1 << kFftOrder;
    static constexpr int kHopSize   = kFftSize / 4; // 4x overlap
    static constexpr int kNumBins   = kFftSize / 2 + 1;

    PitchShifter();

    void  prepare (double sampleRate);
    void  reset();
    void  setPitchRatio (float ratio);

    // Push one input sample, retrieve one output sample (sample-by-sample interface)
    float processSample (float in);

private:
    void processFrame();

    float pitchRatio = 1.0f;

    // Input accumulation — collect kHopSize samples then process
    std::vector<float> inFifo;
    int fifoIdx = 0;

    // Overlap-add output buffer
    std::vector<float> outAccum;     // accumulation buffer
    std::vector<float> outFifo;      // ready-to-read output
    int outIdx = 0;

    // Analysis window history (last kFftSize input samples)
    std::vector<float> inputHistory;
    int historyIdx = 0;

    // FFT
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> fftBuffer;    // 2*kFftSize, in-place work buffer

    // Phase tracking
    std::vector<float> lastAnalysisPhase;
    std::vector<float> synthPhase;

    // Hann window
    std::vector<float> window;

    // Magnitude and true-frequency arrays (reused each frame)
    std::vector<float> mag;
    std::vector<float> trueFreq;
    std::vector<float> outMag;
    std::vector<float> outFreq;

    double sampleRate = 44100.0;
};

// ── Presets ───────────────────────────────────────────────────────────────────
struct ChoirBoxPreset
{
    const char* name;
    float upSemitones;
    float downSemitones;
    float voices;
    float detune;
    float dryLevel;
    float upLevel;
    float downLevel;
    float saturation;
    float crush;
    float distMix;
    float masterOut;
};

static const ChoirBoxPreset kPresets[] =
{
    { "Power Fifth",  7.f,  -7.f, 1.f,  0.f, 1.0f, 0.7f, 0.7f, 0.0f, 0.0f, 0.0f, 1.0f },
    { "Tight Stack",  3.f,  -3.f, 2.f, 15.f, 1.0f, 0.8f, 0.8f, 0.2f, 0.0f, 0.2f, 1.0f },
    { "Choir",        7.f,  -5.f, 4.f, 40.f, 0.8f, 0.9f, 0.9f, 0.1f, 0.0f, 0.1f, 0.8f },
    { "Lo-Fi Choir",  5.f,  -5.f, 3.f, 30.f, 0.8f, 0.9f, 0.9f, 0.4f, 0.3f, 0.5f, 0.8f },
    { "Blown Out",    7.f, -12.f, 2.f, 10.f, 0.6f, 1.0f, 0.8f, 0.6f, 0.8f, 0.8f, 0.7f },
    { "Ghost Voice", 12.f,  -7.f, 1.f,  0.f, 0.3f, 0.9f, 0.7f, 0.3f, 0.0f, 0.3f, 1.0f },
};
static constexpr int kNumPresets = (int)(sizeof(kPresets) / sizeof(kPresets[0]));
static constexpr int kMaxVoices  = 4;

// ── Processor ─────────────────────────────────────────────────────────────────
class ChoirBoxProcessor : public juce::AudioProcessor
{
public:
    ChoirBoxProcessor();
    ~ChoirBoxProcessor() override;

    void prepareToPlay  (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock   (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Choir Box"; }
    bool acceptsMidi()  const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.1; }

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

    int    currentPreset     = 0;
    double currentSampleRate = 44100.0;

    std::array<PitchShifter, kMaxVoices> upShifterL;
    std::array<PitchShifter, kMaxVoices> upShifterR;
    std::array<PitchShifter, kMaxVoices> downShifterL;
    std::array<PitchShifter, kMaxVoices> downShifterR;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChoirBoxProcessor)
};