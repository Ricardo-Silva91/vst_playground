#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;

namespace pid
{
    static constexpr auto drive   = "drive";
    static constexpr auto bias    = "bias";
    static constexpr auto wowDep  = "wowDepth";
    static constexpr auto wowRate = "wowRate";
    static constexpr auto flDep   = "flutterDepth";
    static constexpr auto flRate  = "flutterRate";
    static constexpr auto tone    = "tone";
    static constexpr auto hiss    = "hiss";
    static constexpr auto mix     = "mix";
    static constexpr auto out     = "output";
}

APVTS::ParameterLayout LizardTapeAudioProcessor::createLayout()
{
    using P = juce::AudioParameterFloat;
    using R = juce::NormalisableRange<float>;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    auto db = [] (float lo, float hi) { return R { lo, hi, 0.01f }; };

    params.push_back (std::make_unique<P> (juce::ParameterID { pid::drive, 1 },   "Drive",        db (0.0f, 24.0f), 4.0f));
    params.push_back (std::make_unique<P> (juce::ParameterID { pid::bias, 1 },    "Sat Bias",     R { 0.0f, 1.0f, 0.001f }, 0.15f));
    params.push_back (std::make_unique<P> (juce::ParameterID { pid::wowDep, 1 },  "Wow Depth",    R { 0.0f, 10.0f, 0.01f }, 3.0f));
    params.push_back (std::make_unique<P> (juce::ParameterID { pid::wowRate, 1 }, "Wow Rate",     R { 0.05f, 4.0f, 0.001f, 0.5f }, 0.7f));
    params.push_back (std::make_unique<P> (juce::ParameterID { pid::flDep, 1 },   "Flutter Depth",R { 0.0f, 3.0f, 0.001f }, 0.6f));
    params.push_back (std::make_unique<P> (juce::ParameterID { pid::flRate, 1 },  "Flutter Rate", R { 2.0f, 16.0f, 0.01f }, 8.0f));
    params.push_back (std::make_unique<P> (juce::ParameterID { pid::tone, 1 },    "Tone",         R { 800.0f, 18000.0f, 1.0f, 0.35f }, 6500.0f));
    params.push_back (std::make_unique<P> (juce::ParameterID { pid::hiss, 1 },    "Hiss",         R { 0.0f, 1.0f, 0.001f }, 0.06f));
    params.push_back (std::make_unique<P> (juce::ParameterID { pid::mix, 1 },     "Mix",          R { 0.0f, 1.0f, 0.001f }, 1.0f));
    params.push_back (std::make_unique<P> (juce::ParameterID { pid::out, 1 },     "Output",       db (-24.0f, 12.0f), 0.0f));
    return { params.begin(), params.end() };
}

LizardTapeAudioProcessor::LizardTapeAudioProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{}

lizard::TapeParams LizardTapeAudioProcessor::paramsFromState() const
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    lizard::TapeParams p;
    p.driveDb        = get (pid::drive);
    p.bias           = get (pid::bias);
    p.wowDepthMs     = get (pid::wowDep);
    p.wowRateHz      = get (pid::wowRate);
    p.flutterDepthMs = get (pid::flDep);
    p.flutterRateHz  = get (pid::flRate);
    p.toneHz         = get (pid::tone);
    p.hiss           = get (pid::hiss);
    p.mix            = get (pid::mix);
    p.outDb          = get (pid::out);
    return p;
}

void LizardTapeAudioProcessor::prepareToPlay (double sampleRate, int)
{
    tape.prepare (sampleRate, getTotalNumOutputChannels());
    tape.snapParams (paramsFromState());
}

bool LizardTapeAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (in != out) return false;
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void LizardTapeAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numCh = buffer.getNumChannels();
    const int numSamp = buffer.getNumSamples();
    tape.setParams (paramsFromState());
    tape.process (buffer.getArrayOfWritePointers(), numCh, numSamp);
}

juce::AudioProcessorEditor* LizardTapeAudioProcessor::createEditor()
{
    return new LizardTapeAudioProcessorEditor (*this);
}

void LizardTapeAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void LizardTapeAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LizardTapeAudioProcessor();
}
