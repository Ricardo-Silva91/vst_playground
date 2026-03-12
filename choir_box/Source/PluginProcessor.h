#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>

// ── PitchShifter ──────────────────────────────────────────────────────────────
// Circular buffer with a read pointer that advances at a different rate than
// the write pointer. The ratio between them determines the pitch shift.
// A second read pointer (crossfade grain) eliminates discontinuities when the
// read pointer wraps around.
class PitchShifter
{
public:
    static constexpr int kBufSize  = 8192;   // must be power of 2
    static constexpr int kGrainSize = 1024;  // crossfade window in samples

    PitchShifter();

    void  prepare (double sampleRate);
    void  reset();
    void  setPitchRatio (float ratio);
    float processSample (float in);

private:
    float    pitchRatio  = 1.0f;

    std::array<float, kBufSize> buf {};
    int   writePos  = 0;
    float readPos   = 0.f;
    float readPos2  = 0.f;   // second grain for crossfade
    bool  useSecond = false;

    // Hann crossfade window
    std::array<float, kGrainSize> window {};
    int   grainPos  = 0;     // position within crossfade window
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

    // kMaxVoices shifters per direction, per channel (L/R)
    std::array<PitchShifter, kMaxVoices> upShifterL;
    std::array<PitchShifter, kMaxVoices> upShifterR;
    std::array<PitchShifter, kMaxVoices> downShifterL;
    std::array<PitchShifter, kMaxVoices> downShifterR;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChoirBoxProcessor)
};