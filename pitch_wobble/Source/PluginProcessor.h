#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <random>
#include <array>

class PitchWobbleProcessor : public juce::AudioProcessor
{
public:
    PitchWobbleProcessor();
    ~PitchWobbleProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "PitchWobble"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    struct Preset
    {
        const char* name;
        float depth;       // cents  0.0–30.0
        float rate;        // Hz     0.1–5.0
        float smoothness;  // 0.0–1.0
    };

    static constexpr int NUM_PRESETS = 8;
    static const std::array<Preset, NUM_PRESETS> presets;

    int  getNumPrograms()                              override { return NUM_PRESETS; }
    int  getCurrentProgram()                           override { return currentProgram; }
    void setCurrentProgram (int index)                 override;
    const juce::String getProgramName (int index)      override;
    void changeProgramName (int, const juce::String&)  override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    int currentProgram = 0;

    static constexpr int BUFFER_SIZE = 65536;
    std::vector<float> circularBuffer[2];
    int   writePos = 0;
    float readPos  = 0.0f;

    double currentSampleRate = 44100.0;

    float wobbleTarget   = 0.0f;
    float wobbleCurrent  = 0.0f;
    float wobblePhase    = 0.0f;
    float wobbleInterval = 0.0f;

    std::mt19937 rng;
    std::uniform_real_distribution<float> dist        { -1.0f, 1.0f };
    std::uniform_real_distribution<float> intervalDist{  0.5f, 1.5f };

    void  pickNewTarget (float depth, float rateHz);
    float centsToPitchRatio (float cents);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PitchWobbleProcessor)
};