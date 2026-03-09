#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// Preset table
//==============================================================================
const std::array<PitchWobbleProcessor::Preset, PitchWobbleProcessor::NUM_PRESETS>
PitchWobbleProcessor::presets =
{{
    //  name                 depth   rate    smooth
    //  ── Subtle / natural ──────────────────────────────────────────────────
    {  "Breath of Life",      3.0f,  0.4f,   0.92f  },
    {  "Studio Drift",        6.0f,  0.8f,   0.88f  },
    //  ── Tape / lo-fi ──────────────────────────────────────────────────────
    {  "Worn Cassette",      12.0f,  1.2f,   0.55f  },
    {  "Reel to Reel",       18.0f,  0.6f,   0.40f  },
    //  ── Dramatic / seasick ────────────────────────────────────────────────
    {  "Open Water",         24.0f,  0.3f,   0.95f  },
    {  "The Bends",          28.0f,  0.5f,   0.90f  },
    //  ── Psychedelic / weird ───────────────────────────────────────────────
    {  "Frayed Signal",      20.0f,  3.5f,   0.20f  },
    {  "Total Dissolution",  30.0f,  5.0f,   0.10f  },
}};

//==============================================================================
// Parameter layout
//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
PitchWobbleProcessor::createParameterLayout()
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

//==============================================================================
// Constructor / Destructor
//==============================================================================
PitchWobbleProcessor::PitchWobbleProcessor()
    : AudioProcessor(BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout()),
      rng(std::random_device{}())
{
    for (int ch = 0; ch < 2; ++ch)
        circularBuffer[ch].resize(BUFFER_SIZE, 0.0f);
}

PitchWobbleProcessor::~PitchWobbleProcessor() {}

//==============================================================================
// Preset implementation
//==============================================================================
void PitchWobbleProcessor::setCurrentProgram (int index)
{
    if (index < 0 || index >= NUM_PRESETS) return;
    currentProgram = index;

    const auto& p = presets[(size_t)index];

    auto setParam = [&](const juce::String& id, float value)
    {
        if (auto* param = dynamic_cast<juce::RangedAudioParameter*>
                              (apvts.getParameter (id)))
            param->setValueNotifyingHost (param->convertTo0to1 (value));
    };

    setParam ("depth",  p.depth);
    setParam ("rate",   p.rate);
    setParam ("smooth", p.smoothness);
}

const juce::String PitchWobbleProcessor::getProgramName (int index)
{
    if (index < 0 || index >= NUM_PRESETS) return {};
    return presets[(size_t)index].name;
}

//==============================================================================
// Audio
//==============================================================================
void PitchWobbleProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;
    writePos      = 0;
    readPos       = 0.0f;
    wobbleCurrent = 0.0f;
    wobbleTarget  = 0.0f;
    wobblePhase   = 0.0f;

    for (int ch = 0; ch < 2; ++ch)
        std::fill (circularBuffer[ch].begin(), circularBuffer[ch].end(), 0.0f);

    pickNewTarget (*apvts.getRawParameterValue ("depth"),
                  *apvts.getRawParameterValue ("rate"));
}

void PitchWobbleProcessor::releaseResources() {}

float PitchWobbleProcessor::centsToPitchRatio (float cents)
{
    return std::pow (2.0f, cents / 1200.0f);
}

void PitchWobbleProcessor::pickNewTarget (float depth, float rateHz)
{
    wobbleTarget   = dist (rng) * depth;
    float base     = (float)currentSampleRate / juce::jmax (0.01f, rateHz);
    wobbleInterval = base * intervalDist (rng);
    wobblePhase    = 0.0f;
}

void PitchWobbleProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                         juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numChannels = juce::jmin (buffer.getNumChannels(), 2);
    const int numSamples  = buffer.getNumSamples();

    float depth  = *apvts.getRawParameterValue ("depth");
    float rate   = *apvts.getRawParameterValue ("rate");
    float smooth = *apvts.getRawParameterValue ("smooth");

    float smoothCoeff = 0.9f + smooth * 0.0995f;

    for (int i = 0; i < numSamples; ++i)
    {
        for (int ch = 0; ch < numChannels; ++ch)
            circularBuffer[ch][(size_t)writePos] = buffer.getSample (ch, i);

        wobblePhase += 1.0f;
        if (wobblePhase >= wobbleInterval)
            pickNewTarget (depth, rate);

        wobbleCurrent = wobbleCurrent * smoothCoeff + wobbleTarget * (1.0f - smoothCoeff);

        float pitchRatio = centsToPitchRatio (wobbleCurrent);
        readPos += pitchRatio;

        if (readPos >= (float)BUFFER_SIZE)
            readPos -= (float)BUFFER_SIZE;

        float gap = (float)writePos - readPos;
        if (gap < 0.0f) gap += (float)BUFFER_SIZE;
        if (gap < 4.0f) readPos = (float)((writePos - 4 + BUFFER_SIZE) % BUFFER_SIZE);

        int   readIdx0 = (int)readPos % BUFFER_SIZE;
        int   readIdx1 = (readIdx0 + 1) % BUFFER_SIZE;
        float frac     = readPos - (float)(int)readPos;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            float s0 = circularBuffer[ch][(size_t)readIdx0];
            float s1 = circularBuffer[ch][(size_t)readIdx1];
            buffer.setSample (ch, i, s0 + frac * (s1 - s0));
        }

        writePos = (writePos + 1) % BUFFER_SIZE;
    }
}

//==============================================================================
// State
//==============================================================================
void PitchWobbleProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void PitchWobbleProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PitchWobbleProcessor();
}