#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
ReverseReverbAudioProcessor::ReverseReverbAudioProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    // Register parameters so FL Studio sees them as automatable knobs
    addParameter(roomSize     = new juce::AudioParameterFloat("roomSize",  "Room Size",  0.1f, 1.0f, 0.8f));
    addParameter(wetMix       = new juce::AudioParameterFloat("wetMix",    "Wet Mix",    0.0f, 1.0f, 0.8f));
    addParameter(windowSizeMs = new juce::AudioParameterFloat("windowMs",  "Window (ms)",100.f, 2000.f, 500.f));
}

ReverseReverbAudioProcessor::~ReverseReverbAudioProcessor() {}

//==============================================================================
void ReverseReverbAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    // Convert the window size parameter (ms) to samples
    windowSizeSamples = static_cast<int>((windowSizeMs->get() / 1000.0f) * sampleRate);

    int channels = getTotalNumInputChannels();

    // Allocate all three buffers to the window size
    captureBuffer .setSize(channels, windowSizeSamples, false, true, false);
    reverbBuffer  .setSize(channels, windowSizeSamples, false, true, false);
    playbackBuffer.setSize(channels, windowSizeSamples, false, true, false);

    captureBuffer .clear();
    reverbBuffer  .clear();
    playbackBuffer.clear();

    captureWritePos = 0;
    playbackReadPos = 0;
    isPlayingBack   = false;

    // Configure JUCE's reverb
    reverbParams.roomSize   = roomSize->get();
    reverbParams.wetLevel   = 1.0f;   // We want 100% wet so we can isolate reverb tail
    reverbParams.dryLevel   = 0.0f;
    reverbParams.damping    = 0.5f;
    reverbParams.width      = 1.0f;
    reverb.setParameters(reverbParams);
    reverb.reset();
}

void ReverseReverbAudioProcessor::releaseResources()
{
    captureBuffer .setSize(0, 0);
    reverbBuffer  .setSize(0, 0);
    playbackBuffer.setSize(0, 0);
}

//==============================================================================
// Called when a capture window is full:
// 1. Apply reverb to get the wet-only tail
// 2. Subtract the dry signal to isolate reverb
// 3. Reverse it into playbackBuffer
void ReverseReverbAudioProcessor::processWindow()
{
    int channels = captureBuffer.getNumChannels();

    // --- Step 1: Copy captured dry audio into reverbBuffer ---
    for (int ch = 0; ch < channels; ++ch)
        reverbBuffer.copyFrom(ch, 0, captureBuffer, ch, 0, windowSizeSamples);

    // --- Step 2: Apply reverb (wet only) to reverbBuffer ---
    // Update params in case user moved a knob
    reverbParams.roomSize = roomSize->get();
    reverbParams.wetLevel = 1.0f;
    reverbParams.dryLevel = 0.0f;
    reverb.setParameters(reverbParams);
    reverb.reset(); // reset so each window is independent

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

    // --- Step 3: Subtract the dry signal from the wet to isolate reverb tail ---
    // reverbBuffer = wetReverb - dry  →  pure reverb tail
    for (int ch = 0; ch < channels; ++ch)
    {
        float* reverbData  = reverbBuffer.getWritePointer(ch);
        const float* dryData = captureBuffer.getReadPointer(ch);

        for (int i = 0; i < windowSizeSamples; ++i)
            reverbData[i] -= dryData[i];
    }

    // --- Step 4: Reverse the reverb tail into playbackBuffer ---
    for (int ch = 0; ch < channels; ++ch)
    {
        const float* reverbData   = reverbBuffer.getReadPointer(ch);
        float*       playbackData = playbackBuffer.getWritePointer(ch);

        for (int i = 0; i < windowSizeSamples; ++i)
            playbackData[i] = reverbData[windowSizeSamples - 1 - i];
    }

    // Reset read position so we play from the start of the reversed buffer
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
    float dry = 1.0f - wet; // blend: 0 = all dry, 1 = all reverse reverb

    // Recalculate window size in case parameter changed
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

    // Process sample by sample so we can interleave capture + playback cleanly
    for (int sample = 0; sample < numSamples; ++sample)
    {
        for (int ch = 0; ch < channels; ++ch)
        {
            float inputSample = buffer.getSample(ch, sample);

            // --- Capture incoming dry audio ---
            captureBuffer.setSample(ch, captureWritePos, inputSample);

            // --- Mix output: dry + reversed reverb ---
            float reversedSample = 0.0f;
            if (isPlayingBack && playbackReadPos < windowSizeSamples)
                reversedSample = playbackBuffer.getSample(ch, playbackReadPos);

            buffer.setSample(ch, sample, (dry * inputSample) + (wet * reversedSample));
        }

        // Advance positions
        captureWritePos++;
        if (isPlayingBack) playbackReadPos++;

        // When capture window is full → process it into a new reversed reverb
        if (captureWritePos >= windowSizeSamples)
        {
            processWindow();    // fills playbackBuffer with reversed reverb
            captureWritePos = 0;
        }
    }
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
}

void ReverseReverbAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    juce::MemoryInputStream stream(data, static_cast<size_t>(sizeInBytes), false);
    *roomSize     = stream.readFloat();
    *wetMix       = stream.readFloat();
    *windowSizeMs = stream.readFloat();
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ReverseReverbAudioProcessor();
}
