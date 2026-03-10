#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

// ── Preset data ───────────────────────────────────────────────────────────────
struct ThroughTheWallPreset
{
    const char* name;
    float thickness;
    float bleed;
    float rattle;
    float distance;
};

static constexpr int kNumPresets = 6;
static const ThroughTheWallPreset kPresets[kNumPresets] =
{
    { "Thin Partition",      0.20f, 0.20f, 0.15f, 0.20f },  // cheap drywall, barely filtered
    { "Apartment Next Door", 0.55f, 0.45f, 0.25f, 0.50f },  // classic shared-wall sound
    { "Concrete Block",      0.85f, 0.60f, 0.10f, 0.65f },  // heavy wall, dense rumble
    { "Upstairs Neighbor",   0.70f, 0.75f, 0.20f, 0.80f },  // floor/ceiling, thuddy and diffuse
    { "Rattling Pipes",      0.40f, 0.30f, 0.85f, 0.35f },  // old building, wall buzzes
    { "Ghost Room",          0.95f, 0.85f, 0.05f, 0.95f },  // barely there, pure atmosphere
};

// ── Processor ─────────────────────────────────────────────────────────────────
class ThroughTheWallAudioProcessor : public juce::AudioProcessor
{
public:
    ThroughTheWallAudioProcessor();
    ~ThroughTheWallAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return kNumPresets; }
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram(int index) override { applyPreset(index); }
    const juce::String getProgramName(int index) override;
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    void applyPreset(int index);

    juce::AudioProcessorValueTreeState apvts;

private:
    int currentPreset = 0;
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Wall thickness: multi-stage low-pass filter
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> lowPassL, lowPassR;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> lowPass2L, lowPass2R;

    // Wall rattle: comb filter via short delay lines
    static constexpr int kCombDelaySamples = 512;
    std::array<float, kCombDelaySamples> combBufferL{}, combBufferR{};
    int combWritePos = 0;

    // Room bleed: simple Schroeder reverb-style allpass + comb network
    // We'll use 4 comb filters and 2 allpass filters per channel
    static constexpr int kNumCombs = 4;
    static constexpr int kNumAllpass = 2;

    struct DelayLine {
        std::vector<float> buffer;
        int writePos = 0;
        float feedback = 0.0f;

        void prepare(int size) {
            buffer.assign(size, 0.0f);
            writePos = 0;
        }

        float processComb(float input, float fb) {
            int readPos = writePos;
            float delayed = buffer[readPos];
            float out = input + delayed * fb;
            buffer[writePos] = out;
            writePos = (writePos + 1) % (int)buffer.size();
            return delayed;
        }

        float processAllpass(float input, float coeff) {
            int readPos = writePos;
            float delayed = buffer[readPos];
            float out = -input * coeff + delayed;
            buffer[writePos] = input + delayed * coeff;
            writePos = (writePos + 1) % (int)buffer.size();
            return out;
        }
    };

    // Comb delay sizes (prime-ish, in samples at 44100)
    static constexpr std::array<int, kNumCombs> kCombSizesL = { 1557, 1617, 1491, 1422 };
    static constexpr std::array<int, kNumCombs> kCombSizesR = { 1617, 1557, 1422, 1491 };
    static constexpr std::array<int, kNumAllpass> kAllpassSizes = { 225, 556 };

    std::array<DelayLine, kNumCombs> reverbCombL, reverbCombR;
    std::array<DelayLine, kNumAllpass> reverbAllpassL, reverbAllpassR;

    // Distance: simple gain + subtle stereo widener
    float smoothedThickness = 0.5f;
    float smoothedBleed = 0.3f;
    float smoothedRattle = 0.2f;
    float smoothedDistance = 0.5f;

    double currentSampleRate = 44100.0;

    void updateFilters();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ThroughTheWallAudioProcessor)
};