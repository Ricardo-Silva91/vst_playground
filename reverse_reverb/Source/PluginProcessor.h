#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

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
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Parameters exposed to the UI
    juce::AudioParameterFloat* roomSize;
    juce::AudioParameterFloat* wetMix;
    juce::AudioParameterFloat* windowSizeMs; // how long a reverb capture window is (ms)

private:
    // --- Core buffers ---
    // captureBuffer: records incoming dry audio for one full window
    juce::AudioBuffer<float> captureBuffer;

    // reverbBuffer: holds the reverb-only tail (wet - dry), then gets reversed
    juce::AudioBuffer<float> reverbBuffer;

    // playbackBuffer: the final reversed reverb, played out while next window captures
    juce::AudioBuffer<float> playbackBuffer;

    // --- State ---
    int captureWritePos  = 0;   // where we are writing in the capture window
    int playbackReadPos  = 0;   // where we are reading the reversed reverb output
    int windowSizeSamples = 0;  // total samples in one window
    bool isPlayingBack   = false;

    double currentSampleRate = 44100.0;

    // JUCE's built-in reverb processor
    juce::Reverb reverb;
    juce::Reverb::Parameters reverbParams;

    void processWindow(); // called when a capture window is full

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReverseReverbAudioProcessor)
};