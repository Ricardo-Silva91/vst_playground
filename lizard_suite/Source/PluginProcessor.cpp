#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "SharedProcessorUtils.h"
#include <cmath>

// ── Parameter IDs ─────────────────────────────────────────────────────────────
static const juce::String kDustRate    = "dustRate";
static const juce::String kDustBits    = "dustBits";
static const juce::String kDustLowpass = "dustLowpass";
static const juce::String kDustDrive   = "dustDrive";
static const juce::String kDustMix     = "dustMix";
static const juce::String kChewRate    = "chewRate";
static const juce::String kChewDepth   = "chewDepth";
static const juce::String kChewSmooth  = "chewSmooth";
static const juce::String kMurkRoom    = "murkRoom";
static const juce::String kMurkDamp    = "murkDamp";
static const juce::String kMurkMix     = "murkMix";
static const juce::String kVinylAge    = "vinylAge";
static const juce::String kVinylAmount = "vinylAmount";
static const juce::String kVinylSeed   = "vinylSeed";

LizardSuiteProcessor::LizardSuiteProcessor()
    : AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
}

LizardSuiteProcessor::~LizardSuiteProcessor() {}

juce::AudioProcessorValueTreeState::ParameterLayout
LizardSuiteProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // ── Dust — same ranges, defaults and skews as Lizard Dust ────────────────
    juce::NormalisableRange<float> rateRange (1000.f, 48000.f, 1.f);
    rateRange.setSkewForCentre (12000.f);
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (kDustRate,    "Dust Rate",     rateRange, 26040.f));
    params.push_back (std::make_unique<juce::AudioParameterInt>
        (kDustBits,    "Dust Bits",     2, 16, 12));
    juce::NormalisableRange<float> lpRange (0.f, 20000.f, 1.f);   // 0 = off
    lpRange.setSkewForCentre (4000.f);
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (kDustLowpass, "Dust Low-pass", lpRange, 0.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (kDustDrive,   "Dust Drive",    juce::NormalisableRange<float> (1.f, 8.f, 0.01f, 0.5f), 1.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (kDustMix,     "Dust Mix",      juce::NormalisableRange<float> (0.f, 1.f, 0.001f), 1.f));

    // ── Chew ─────────────────────────────────────────────────────────────────
    juce::NormalisableRange<float> chewRateRange (0.f, 20.f, 0.01f);
    chewRateRange.setSkewForCentre (4.f);
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (kChewRate,    "Chew Rate",     chewRateRange, 3.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (kChewDepth,   "Chew Depth",    juce::NormalisableRange<float> (0.f, 1.f, 0.001f), 0.5f));
    juce::NormalisableRange<float> smoothRange (1.f, 50.f, 0.1f);
    smoothRange.setSkewForCentre (10.f);
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (kChewSmooth,  "Chew Smooth",   smoothRange, 8.f));

    // ── Murk ─────────────────────────────────────────────────────────────────
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (kMurkRoom,    "Murk Room",     juce::NormalisableRange<float> (0.f, 1.f, 0.001f), 0.6f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (kMurkDamp,    "Murk Damp",     juce::NormalisableRange<float> (0.f, 1.f, 0.001f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (kMurkMix,     "Murk Mix",      juce::NormalisableRange<float> (0.f, 1.f, 0.001f), 0.3f));

    // ── Vinyl ────────────────────────────────────────────────────────────────
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (kVinylAge,    "Vinyl Age",     juce::NormalisableRange<float> (0.f, 1.f, 0.001f), 0.4f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (kVinylAmount, "Vinyl Amount",  juce::NormalisableRange<float> (0.f, 1.f, 0.001f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterInt>
        (kVinylSeed,   "Vinyl Seed",    1, 999, 1));

    return { params.begin(), params.end() };
}

void LizardSuiteProcessor::reseed()
{
    lastVinylSeed = (int) std::lround (apvts.getRawParameterValue (kVinylSeed)->load());
    for (int ch = 0; ch < kMaxChannels; ++ch)
    {
        chew [(size_t) ch].setSeed (kChewSeed);
        vinyl[(size_t) ch].setSeed (vinylSeedFor (lastVinylSeed, ch));
    }
}

void LizardSuiteProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    for (int ch = 0; ch < kMaxChannels; ++ch)
    {
        const auto c = (size_t) ch;
        dust [c].setHostSampleRate (sampleRate);  dust [c].reset();
        chew [c].setHostSampleRate (sampleRate);  chew [c].reset();
        murk [c].setHostSampleRate (sampleRate);  murk [c].reset();   // allocates here, never in process()
        vinyl[c].setHostSampleRate (sampleRate);  vinyl[c].reset();
    }
    reseed();
}

bool LizardSuiteProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void LizardSuiteProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int n      = buffer.getNumSamples();

    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, n);

    auto get = [this] (const juce::String& id) { return apvts.getRawParameterValue (id)->load(); };

    // Re-seed only when the knob moves: seeding every block would restart
    // the noise stream and turn the bed into a buzz at the block rate.
    if ((int) std::lround (get (kVinylSeed)) != lastVinylSeed)
        reseed();

    const int channels = juce::jmin (numOut, kMaxChannels);
    for (int ch = 0; ch < channels; ++ch)
    {
        const auto c = (size_t) ch;
        float* x = buffer.getWritePointer (ch);

        dust[c].setTargetRate (get (kDustRate));
        dust[c].setBitDepth   ((int) std::lround (get (kDustBits)));
        dust[c].setLowpassHz  (get (kDustLowpass));
        dust[c].setDrive      (get (kDustDrive));
        dust[c].setMix        (get (kDustMix));
        dust[c].process (x, n);

        chew[c].setRate     (get (kChewRate));
        chew[c].setDepth    (get (kChewDepth));
        chew[c].setSmoothMs (get (kChewSmooth));
        chew[c].process (x, n);

        murk[c].setRoomSize (get (kMurkRoom));
        murk[c].setDamp     (get (kMurkDamp));
        murk[c].setMix      (get (kMurkMix));
        murk[c].process (x, n);

        vinyl[c].setAge    (get (kVinylAge));
        vinyl[c].setAmount (get (kVinylAmount));
        vinyl[c].process (x, n);
    }
}

void LizardSuiteProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    SharedProcessorUtils::saveState (*this, apvts, destData);
}

void LizardSuiteProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    SharedProcessorUtils::loadState (*this, apvts, data, sizeInBytes);
}

juce::AudioProcessorEditor* LizardSuiteProcessor::createEditor()
{
    return new LizardSuiteEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LizardSuiteProcessor();
}
