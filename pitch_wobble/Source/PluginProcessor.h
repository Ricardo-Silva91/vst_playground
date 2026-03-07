#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <random>

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

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Pitch shifting via resampling
    // We maintain a circular buffer and read at a varying speed
    static constexpr int BUFFER_SIZE = 65536; // ~1.5s at 44100
    std::vector<float> circularBuffer[2];
    int writePos = 0;
    float readPos = 0.0f;

    // Wobble LFO state
    double currentSampleRate = 44100.0;

    // Smooth random target wobble
    float wobbleTarget = 0.0f;    // target pitch offset in cents
    float wobbleCurrent = 0.0f;   // current smoothed pitch offset in cents
    float wobblePhase = 0.0f;     // timer for when to pick new target
    float wobbleInterval = 0.0f;  // samples until next target

    std::mt19937 rng;
    std::uniform_real_distribution<float> dist{ -1.0f, 1.0f };
    std::uniform_real_distribution<float> intervalDist{ 0.5f, 1.5f }; // multiplier for interval variation

    void pickNewTarget(float depth, float rateHz);
    float centsToPitchRatio(float cents);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PitchWobbleProcessor)
};
