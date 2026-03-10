#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout ThroughTheWallAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "thickness", "Wall Thickness",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "bleed", "Room Bleed",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.35f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "rattle", "Wall Rattle",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.2f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "distance", "Distance",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.5f));

    return { params.begin(), params.end() };
}

ThroughTheWallAudioProcessor::ThroughTheWallAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
}

ThroughTheWallAudioProcessor::~ThroughTheWallAudioProcessor() {}

void ThroughTheWallAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32)samplesPerBlock;
    spec.numChannels = 1;

    lowPassL.prepare(spec);
    lowPassR.prepare(spec);
    lowPass2L.prepare(spec);
    lowPass2R.prepare(spec);

    // Scale reverb delay lines to sample rate
    double srRatio = sampleRate / 44100.0;

    for (int i = 0; i < kNumCombs; ++i) {
        reverbCombL[i].prepare((int)(kCombSizesL[i] * srRatio));
        reverbCombR[i].prepare((int)(kCombSizesR[i] * srRatio));
    }
    for (int i = 0; i < kNumAllpass; ++i) {
        reverbAllpassL[i].prepare((int)(kAllpassSizes[i] * srRatio));
        reverbAllpassR[i].prepare((int)(kAllpassSizes[i] * srRatio));
    }

    combBufferL.fill(0.0f);
    combBufferR.fill(0.0f);
    combWritePos = 0;

    updateFilters();
}

void ThroughTheWallAudioProcessor::releaseResources() {}

void ThroughTheWallAudioProcessor::updateFilters()
{
    // Wall thickness maps to low-pass cutoff: 0 = 4000 Hz, 1 = 150 Hz
    float thickness = smoothedThickness;
    float cutoff = std::exp(std::log(4000.0f) + thickness * (std::log(150.0f) - std::log(4000.0f)));
    cutoff = juce::jlimit(80.0f, 18000.0f, cutoff);

    *lowPassL.state  = *juce::dsp::IIR::Coefficients<float>::makeLowPass(currentSampleRate, cutoff, 0.7f);
    *lowPassR.state  = *juce::dsp::IIR::Coefficients<float>::makeLowPass(currentSampleRate, cutoff, 0.7f);
    *lowPass2L.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(currentSampleRate, cutoff * 0.7f, 0.9f);
    *lowPass2R.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(currentSampleRate, cutoff * 0.7f, 0.9f);
}

void ThroughTheWallAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    float targetThickness = apvts.getRawParameterValue("thickness")->load();
    float targetBleed     = apvts.getRawParameterValue("bleed")->load();
    float targetRattle    = apvts.getRawParameterValue("rattle")->load();
    float targetDistance  = apvts.getRawParameterValue("distance")->load();

    const float smoothCoeff = 0.005f;
    bool filterNeedsUpdate = std::abs(targetThickness - smoothedThickness) > 0.001f;
    smoothedThickness += smoothCoeff * (targetThickness - smoothedThickness);
    smoothedBleed     += smoothCoeff * (targetBleed     - smoothedBleed);
    smoothedRattle    += smoothCoeff * (targetRattle    - smoothedRattle);
    smoothedDistance  += smoothCoeff * (targetDistance  - smoothedDistance);

    if (filterNeedsUpdate)
        updateFilters();

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (numChannels < 2) return;

    float* dataL = buffer.getWritePointer(0);
    float* dataR = buffer.getWritePointer(1);

    // ---- Distance attenuation: dry signal dims as Distance increases ----
    // distance=0: pass-through feel, distance=1: very far away
    float dryGain = std::pow(1.0f - smoothedDistance * 0.85f, 2.0f);

    // ---- Reverb feedback (room bleed) settings ----
    float reverbFeedback = 0.65f + smoothedBleed * 0.3f; // 0.65 - 0.95
    float reverbWet = smoothedBleed * 0.7f * (0.3f + smoothedDistance * 0.7f);

    // ---- Rattle: comb filter depth and delay ----
    // Delay in samples: 2ms-8ms for "wall resonance"
    int rattleDelaySamples = (int)(currentSampleRate * (0.002 + smoothedRattle * 0.006));
    rattleDelaySamples = juce::jlimit(1, kCombDelaySamples - 1, rattleDelaySamples);
    float rattleFeedback = smoothedRattle * 0.45f;
    float rattleWet = smoothedRattle * 0.3f;

    for (int i = 0; i < numSamples; ++i)
    {
        float inL = dataL[i];
        float inR = dataR[i];

        // --- Wall Rattle (comb filter) ---
        int readPos = (combWritePos - rattleDelaySamples + kCombDelaySamples) % kCombDelaySamples;
        float rattledL = inL + combBufferL[readPos] * rattleFeedback;
        float rattledR = inR + combBufferR[readPos] * rattleFeedback;
        combBufferL[combWritePos] = rattledL;
        combBufferR[combWritePos] = rattledR;
        combWritePos = (combWritePos + 1) % kCombDelaySamples;

        float processedL = inL + (rattledL - inL) * rattleWet;
        float processedR = inR + (rattledR - inR) * rattleWet;

        // --- Room Bleed (Schroeder reverb) ---
        float revInL = processedL * 0.015f;
        float revInR = processedR * 0.015f;

        float combSumL = 0.0f, combSumR = 0.0f;
        for (int c = 0; c < kNumCombs; ++c) {
            combSumL += reverbCombL[c].processComb(revInL, reverbFeedback);
            combSumR += reverbCombR[c].processComb(revInR, reverbFeedback);
        }
        combSumL *= 0.25f;
        combSumR *= 0.25f;

        for (int a = 0; a < kNumAllpass; ++a) {
            combSumL = reverbAllpassL[a].processAllpass(combSumL, 0.5f);
            combSumR = reverbAllpassR[a].processAllpass(combSumR, 0.5f);
        }

        processedL = processedL * dryGain + combSumL * reverbWet;
        processedR = processedR * dryGain + combSumR * reverbWet;

        dataL[i] = processedL;
        dataR[i] = processedR;
    }

    // --- Low-pass filtering (Wall Thickness) ---
    // Process each channel through two cascaded low-pass filters
    {
        juce::dsp::AudioBlock<float> blockL(&dataL, 1, (size_t)numSamples);
        juce::dsp::ProcessContextReplacing<float> ctxL(blockL);
        lowPassL.process(ctxL);
        lowPass2L.process(ctxL);
    }
    {
        juce::dsp::AudioBlock<float> blockR(&dataR, 1, (size_t)numSamples);
        juce::dsp::ProcessContextReplacing<float> ctxR(blockR);
        lowPassR.process(ctxR);
        lowPass2R.process(ctxR);
    }
}

const juce::String ThroughTheWallAudioProcessor::getProgramName(int index)
{
    if (index >= 0 && index < kNumPresets)
        return kPresets[index].name;
    return {};
}

void ThroughTheWallAudioProcessor::applyPreset(int index)
{
    if (index < 0 || index >= kNumPresets) return;
    currentPreset = index;
    const auto& p = kPresets[index];
    apvts.getParameter("thickness")->setValueNotifyingHost(p.thickness);
    apvts.getParameter("bleed")    ->setValueNotifyingHost(p.bleed);
    apvts.getParameter("rattle")   ->setValueNotifyingHost(p.rattle);
    apvts.getParameter("distance") ->setValueNotifyingHost(p.distance);
}

void ThroughTheWallAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void ThroughTheWallAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState && xmlState->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

juce::AudioProcessorEditor* ThroughTheWallAudioProcessor::createEditor()
{
    return new ThroughTheWallAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ThroughTheWallAudioProcessor();
}