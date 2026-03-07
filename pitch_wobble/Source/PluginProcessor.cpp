#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout PitchWobbleProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "depth", "Wobble Depth",
        juce::NormalisableRange<float>(0.0f, 30.0f, 0.1f), 8.0f,
        juce::AudioParameterFloatAttributes().withLabel("cents")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "rate", "Wobble Rate",
        juce::NormalisableRange<float>(0.1f, 5.0f, 0.01f), 1.0f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "smooth", "Smoothness",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.7f,
        juce::AudioParameterFloatAttributes().withLabel("")));

    return { params.begin(), params.end() };
}

PitchWobbleProcessor::PitchWobbleProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout()),
      rng(std::random_device{}())
{
    for (int ch = 0; ch < 2; ++ch)
        circularBuffer[ch].resize(BUFFER_SIZE, 0.0f);
}

PitchWobbleProcessor::~PitchWobbleProcessor() {}

void PitchWobbleProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate;
    writePos = 0;
    readPos = 0.0f;
    wobbleCurrent = 0.0f;
    wobbleTarget = 0.0f;
    wobblePhase = 0.0f;

    for (int ch = 0; ch < 2; ++ch)
        std::fill(circularBuffer[ch].begin(), circularBuffer[ch].end(), 0.0f);

    float rate = *apvts.getRawParameterValue("rate");
    float depth = *apvts.getRawParameterValue("depth");
    pickNewTarget(depth, rate);
}

void PitchWobbleProcessor::releaseResources() {}

float PitchWobbleProcessor::centsToPitchRatio(float cents)
{
    return std::pow(2.0f, cents / 1200.0f);
}

void PitchWobbleProcessor::pickNewTarget(float depth, float rateHz)
{
    wobbleTarget = dist(rng) * depth;
    float baseSamples = (float)currentSampleRate / juce::jmax(0.01f, rateHz);
    wobbleInterval = baseSamples * intervalDist(rng); // vary interval slightly for organic feel
    wobblePhase = 0.0f;
}

void PitchWobbleProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numChannels = juce::jmin(buffer.getNumChannels(), 2);
    const int numSamples = buffer.getNumSamples();

    float depth   = *apvts.getRawParameterValue("depth");
    float rate    = *apvts.getRawParameterValue("rate");
    float smooth  = *apvts.getRawParameterValue("smooth");

    // smooth factor: higher smooth value = slower chase = more glide
    // map 0..1 -> coeff range e.g. 0.9995 (very slow) to 0.9 (fast)
    float smoothCoeff = 0.9f + smooth * 0.0995f;

    for (int i = 0; i < numSamples; ++i)
    {
        // Write input into circular buffer
        for (int ch = 0; ch < numChannels; ++ch)
            circularBuffer[ch][writePos] = buffer.getSample(ch, i);

        // Update wobble target timer
        wobblePhase += 1.0f;
        if (wobblePhase >= wobbleInterval)
            pickNewTarget(depth, rate);

        // Smooth toward target
        wobbleCurrent = wobbleCurrent * smoothCoeff + wobbleTarget * (1.0f - smoothCoeff);

        // Convert cents to read speed ratio
        float pitchRatio = centsToPitchRatio(wobbleCurrent);

        // Advance read position by pitchRatio
        readPos += pitchRatio;

        // Keep read position behind write position by at least a small margin
        // and wrap within buffer
        if (readPos >= (float)BUFFER_SIZE)
            readPos -= (float)BUFFER_SIZE;

        // Clamp: ensure readPos doesn't lap writePos
        // If too close, nudge readPos back slightly
        float gap = (float)writePos - readPos;
        if (gap < 0.0f) gap += (float)BUFFER_SIZE;
        if (gap < 4.0f) readPos = (float)((writePos - 4 + BUFFER_SIZE) % BUFFER_SIZE);

        // Linear interpolation read
        int readIdx0 = (int)readPos % BUFFER_SIZE;
        int readIdx1 = (readIdx0 + 1) % BUFFER_SIZE;
        float frac = readPos - (float)(int)readPos;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            float s0 = circularBuffer[ch][readIdx0];
            float s1 = circularBuffer[ch][readIdx1];
            buffer.setSample(ch, i, s0 + frac * (s1 - s0));
        }

        writePos = (writePos + 1) % BUFFER_SIZE;
    }
}

void PitchWobbleProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void PitchWobbleProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState && xmlState->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PitchWobbleProcessor();
}
