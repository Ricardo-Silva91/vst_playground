#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>
#include <complex>

// ── Pitch shifter — overlap-add phase vocoder, mono ───────────────────────────
class PitchShifter
{
public:
    static constexpr int kFftSize    = 2048;
    static constexpr int kHopSize    = kFftSize / 4;
    static constexpr int kOverlap    = kFftSize / kHopSize;

    PitchShifter();

    void prepare (double sampleRate);
    void reset();
    void setPitchRatio (float ratio);  // e.g. 1.5f = up a fifth
    float processSample (float in);    // push one sample, pop one sample

private:
    void processFrame();

    double sampleRate = 44100.0;
    float  pitchRatio = 1.0f;

    // Input / output circular buffers
    std::vector<float> inBuf;
    std::vector<float> outBuf;
    int inWritePos  = 0;
    int outReadPos  = 0;
    int outWritePos = 0;
    int inputFill   = 0;   // samples accumulated since last frame

    // FFT
    std::unique_ptr<juce::dsp::FFT> fft;

    // Phase accumulators
    std::vector<float> lastPhase;
    std::vector<float> synthPhase;

    // Hann window
    std::vector<float> window;

    // Work buffers
    std::vector<float>               timeDomain;
    std::vector<std::complex<float>> freqDomain;

    int outputLatency = 0;  // samples buffered before first output
};

// ── Presets ───────────────────────────────────────────────────────────────────
struct ChoirBoxPreset
{
    const char* name;
    float upSemitones;    // -24 to +24
    float downSemitones;  // -24 to +24
    float voices;         // 1 to 4
    float detune;         // 0 to 100 cents
    float dryLevel;       // 0 to 1
    float upLevel;        // 0 to 1
    float downLevel;      // 0 to 1
    float saturation;     // 0 to 1
    float crush;          // 0 to 1
    float distMix;        // 0 to 1
    float masterOut;      // 0 to 2
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

// Max voice pairs — 4 up + 4 down = 8 pitch shifter pairs (stereo = 16 instances)
static constexpr int kMaxVoices = 4;

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
    double getTailLengthSeconds() const override { return 0.05; }

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

    // kMaxVoices pitch shifters per side (up/down), stereo (L/R)
    std::array<PitchShifter, kMaxVoices> upShifterL;
    std::array<PitchShifter, kMaxVoices> upShifterR;
    std::array<PitchShifter, kMaxVoices> downShifterL;
    std::array<PitchShifter, kMaxVoices> downShifterR;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChoirBoxProcessor)
};