#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>

// ── PitchShifter — dual-grain overlap-add ─────────────────────────────────────
// Two grains read from a circular delay buffer at a speed determined by
// pitchRatio. Each grain is windowed with a Hann envelope. When a grain
// completes its window it jumps forward so the two grains are always
// kGrainSize apart and their envelopes always sum to 1.0 (no gaps, no clicks).
//
// kBufSize  — delay buffer length, must be power of 2, >> kGrainSize
// kGrainSize — length of each crossfade grain in samples
class PitchShifter
{
public:
    static constexpr int kBufSize   = 16384;  // power of 2, ~370ms @44.1k
    static constexpr int kGrainSize = 2048;   // ~46ms — long enough for smoothness

    PitchShifter();

    void  prepare (double sampleRate);
    void  reset();
    void  setPitchRatio (float ratio);
    float processSample (float in);

private:
    float pitchRatio = 1.0f;

    // Circular input buffer
    std::array<float, kBufSize> buf {};
    int writePos = 0;

    // Two grains — each has a floating-point read position and an envelope phase
    struct Grain
    {
        float readPos  = 0.f;   // fractional position in buf
        int   envPos   = 0;     // position within Hann window (0..kGrainSize-1)
    };
    Grain grains[2];

    // Hann window lookup table
    std::array<float, kGrainSize> hann {};

    // Read one sample from buf at a fractional position (linear interp)
    float readAt (float pos) const;
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

    std::array<PitchShifter, kMaxVoices> upShifterL;
    std::array<PitchShifter, kMaxVoices> upShifterR;
    std::array<PitchShifter, kMaxVoices> downShifterL;
    std::array<PitchShifter, kMaxVoices> downShifterR;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChoirBoxProcessor)
};