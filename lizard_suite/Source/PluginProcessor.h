#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <cstdint>
#include "DSP/LizardDust.h"
#include "DSP/LizardChew.h"
#include "DSP/LizardMurk.h"
#include "DSP/LizardVinyl.h"

// ── Lizard Suite ──────────────────────────────────────────────────────────────
// Four JUCE-free lo-fi cores in series, one instance of each per channel:
//
//   Dust (sampler) → Chew (tape) → Murk (room) → Vinyl (record)
//
// Parameters are applied once per block, unsmoothed, so each module's "off"
// setting (Dust Mix 0 / Chew Depth 0 / Murk Mix 0 / Vinyl Amount 0) is a
// bit-identical dry path.
class LizardSuiteProcessor : public juce::AudioProcessor
{
public:
    LizardSuiteProcessor();
    ~LizardSuiteProcessor() override;

    void prepareToPlay  (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock   (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Lizard Suite"; }
    bool acceptsMidi()  const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int  getNumPrograms()    override { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram  (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName  (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // Chew's dropouts share one seed so both channels sag together, like a
    // real tape losing head contact. Vinyl gets a different stream per channel
    // so the hiss is stereo.
    static constexpr uint32_t kChewSeed = 99991u;
    static uint32_t vinylSeedFor (int seed, int channel) noexcept
    {
        return (uint32_t) seed * 2654435761u + (uint32_t) channel * 40503u;
    }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void reseed();

    static constexpr int kMaxChannels = 2;
    std::array<LizardDust,  kMaxChannels> dust;
    std::array<LizardChew,  kMaxChannels> chew;
    std::array<LizardMurk,  kMaxChannels> murk;
    std::array<LizardVinyl, kMaxChannels> vinyl;

    int lastVinylSeed = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LizardSuiteProcessor)
};
