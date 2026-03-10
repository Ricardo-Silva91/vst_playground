#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

// ── Preset data ───────────────────────────────────────────────────────────────
struct ReverseReverbPreset
{
    const char* name;
    float roomSize;
    float wetMix;
    float windowMs;
};

static constexpr int kNumPresets = 5;
static const ReverseReverbPreset kPresets[kNumPresets] =
{
    { "Default",        0.8f,  0.8f,  500.0f },
    { "Ghost Bloom",    0.95f, 0.9f,  800.0f },  // large room, long swell
    { "Subtle Breath",  0.5f,  0.4f,  300.0f },  // small room, gentle pre-verb
    { "Wash Out",       1.0f,  1.0f, 1500.0f },  // full wet, very long window
    { "Tight Shimmer",  0.6f,  0.7f,  150.0f },  // short window, punchy
};

// ── Processor ─────────────────────────────────────────────────────────────────
class ReverseReverbAudioProcessor : public juce::AudioProcessor
{
public:
    ReverseReverbAudioProcessor();
    ~ReverseReverbAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "ReverseReverb"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int  getNumPrograms()                               override { return kNumPresets; }
    int  getCurrentProgram()                            override { return currentPreset; }
    void setCurrentProgram(int index)                   override { applyPreset(index); }
    const juce::String getProgramName(int index)        override;
    void changeProgramName(int, const juce::String&)    override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    void applyPreset(int index);

    juce::AudioParameterFloat* roomSize;
    juce::AudioParameterFloat* wetMix;
    juce::AudioParameterFloat* windowSizeMs;

private:
    int currentPreset = 0;

    juce::AudioBuffer<float> captureBuffer;
    juce::AudioBuffer<float> reverbBuffer;
    juce::AudioBuffer<float> playbackBuffer;

    int captureWritePos   = 0;
    int playbackReadPos   = 0;
    int windowSizeSamples = 0;
    bool isPlayingBack    = false;

    double currentSampleRate = 44100.0;

    juce::Reverb reverb;
    juce::Reverb::Parameters reverbParams;

    void processWindow();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReverseReverbAudioProcessor)
};