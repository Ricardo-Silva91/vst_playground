#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
ReverseReverbAudioProcessor::ReverseReverbAudioProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    // Plain string IDs (no version hints) for reliable FL Studio automation mapping
    addParameter(roomSize     = new juce::AudioParameterFloat("roomSize",  "Room Size",  0.1f, 1.0f, 0.8f));
    addParameter(wetMix       = new juce::AudioParameterFloat("wetMix",    "Wet Mix",    0.0f, 1.0f, 0.8f));
    addParameter(windowSizeMs = new juce::AudioParameterFloat("windowMs",  "Window (ms)", 100.f, 2000.f, 500.f));
}

ReverseReverbAudioProcessor::~ReverseReverbAudioProcessor() {}

//==============================================================================
void ReverseReverbAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    windowSizeSamples = static_cast<int>((windowSizeMs->get() / 1000.0f) * sampleRate);

    int channels = getTotalNumInputChannels();

    captureBuffer .setSize(channels, windowSizeSamples, false, true, false);
    reverbBuffer  .setSize(channels, windowSizeSamples, false, true, false);
    playbackBuffer.setSize(channels, windowSizeSamples, false, true, false);

    captureBuffer .clear();
    reverbBuffer  .clear();
    playbackBuffer.clear();

    captureWritePos = 0;
    playbackReadPos = 0;
    isPlayingBack   = false;

    reverbParams.roomSize = roomSize->get();
    reverbParams.wetLevel = 1.0f;
    reverbParams.dryLevel = 0.0f;
    reverbParams.damping  = 0.5f;
    reverbParams.width    = 1.0f;
    reverb.setParameters(reverbParams);
    reverb.setSampleRate(sampleRate); // use setSampleRate, not prepare(spec)
    reverb.reset();
}

void ReverseReverbAudioProcessor::releaseResources()
{
    captureBuffer .setSize(0, 0);
    reverbBuffer  .setSize(0, 0);
    playbackBuffer.setSize(0, 0);
}

//==============================================================================
void ReverseReverbAudioProcessor::processWindow()
{
    int channels = captureBuffer.getNumChannels();

    for (int ch = 0; ch < channels; ++ch)
        reverbBuffer.copyFrom(ch, 0, captureBuffer, ch, 0, windowSizeSamples);

    reverbParams.roomSize = roomSize->get();
    reverbParams.wetLevel = 1.0f;
    reverbParams.dryLevel = 0.0f;
    reverb.setParameters(reverbParams);
    reverb.reset();

    if (channels == 2)
    {
        reverb.processStereo(reverbBuffer.getWritePointer(0),
                             reverbBuffer.getWritePointer(1),
                             windowSizeSamples);
    }
    else
    {
        reverb.processMono(reverbBuffer.getWritePointer(0), windowSizeSamples);
    }

    for (int ch = 0; ch < channels; ++ch)
    {
        float*       reverbData = reverbBuffer.getWritePointer(ch);
        const float* dryData    = captureBuffer.getReadPointer(ch);

        for (int i = 0; i < windowSizeSamples; ++i)
            reverbData[i] -= dryData[i];
    }

    for (int ch = 0; ch < channels; ++ch)
    {
        const float* reverbData   = reverbBuffer.getReadPointer(ch);
        float*       playbackData = playbackBuffer.getWritePointer(ch);

        for (int i = 0; i < windowSizeSamples; ++i)
            playbackData[i] = reverbData[windowSizeSamples - 1 - i];
    }

    playbackReadPos = 0;
    isPlayingBack   = true;
}

//==============================================================================
void ReverseReverbAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    int numSamples = buffer.getNumSamples();
    int channels   = juce::jmin(buffer.getNumChannels(), captureBuffer.getNumChannels());

    float wet = wetMix->get();
    float dry = 1.0f - wet;

    int newWindowSize = static_cast<int>((windowSizeMs->get() / 1000.0f) * currentSampleRate);
    if (newWindowSize != windowSizeSamples)
    {
        windowSizeSamples = newWindowSize;
        captureBuffer .setSize(channels, windowSizeSamples, false, true, true);
        reverbBuffer  .setSize(channels, windowSizeSamples, false, true, true);
        playbackBuffer.setSize(channels, windowSizeSamples, false, true, true);
        captureWritePos = 0;
        playbackReadPos = 0;
        isPlayingBack   = false;
    }

    for (int sample = 0; sample < numSamples; ++sample)
    {
        for (int ch = 0; ch < channels; ++ch)
        {
            float inputSample = buffer.getSample(ch, sample);

            captureBuffer.setSample(ch, captureWritePos, inputSample);

            float reversedSample = 0.0f;
            if (isPlayingBack && playbackReadPos < windowSizeSamples)
                reversedSample = playbackBuffer.getSample(ch, playbackReadPos);

            buffer.setSample(ch, sample, (dry * inputSample) + (wet * reversedSample));
        }

        captureWritePos++;
        if (isPlayingBack) playbackReadPos++;

        if (captureWritePos >= windowSizeSamples)
        {
            processWindow();
            captureWritePos = 0;
        }
    }
}

//==============================================================================
// Presets
//==============================================================================
void ReverseReverbAudioProcessor::applyPreset(int index)
{
    if (index < 0 || index >= kNumPresets) return;
    currentPreset = index;
    const auto& p = kPresets[index];

    auto set = [](juce::AudioParameterFloat* param, float value) {
        param->setValueNotifyingHost(param->convertTo0to1(value));
    };

    set(roomSize,     p.roomSize);
    set(wetMix,       p.wetMix);
    set(windowSizeMs, p.windowMs);
}

const juce::String ReverseReverbAudioProcessor::getProgramName(int index)
{
    if (index >= 0 && index < kNumPresets)
        return kPresets[index].name;
    return "Unknown";
}

//==============================================================================
juce::AudioProcessorEditor* ReverseReverbAudioProcessor::createEditor()
{
    return new ReverseReverbAudioProcessorEditor(*this);
}

void ReverseReverbAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream stream(destData, true);
    stream.writeFloat(roomSize->get());
    stream.writeFloat(wetMix->get());
    stream.writeFloat(windowSizeMs->get());
    stream.writeInt(currentPreset);
}

void ReverseReverbAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    juce::MemoryInputStream stream(data, static_cast<size_t>(sizeInBytes), false);
    *roomSize     = stream.readFloat();
    *wetMix       = stream.readFloat();
    *windowSizeMs = stream.readFloat();
    currentPreset = stream.readInt();
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ReverseReverbAudioProcessor();
}