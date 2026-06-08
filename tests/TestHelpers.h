#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>

static inline juce::AudioBuffer<float> makeSilent(int channels = 2, int samples = 512)
{
    juce::AudioBuffer<float> buf(channels, samples);
    buf.clear();
    return buf;
}

static inline void fillWithSine(juce::AudioBuffer<float>& buf, float freqHz, double sampleRate, float amp = 0.5f)
{
    const float twoPi = juce::MathConstants<float>::twoPi;
    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        for (int i = 0; i < buf.getNumSamples(); ++i)
            buf.setSample(ch, i, amp * std::sin(twoPi * freqHz * (float)i / (float)sampleRate));
}

static inline float rmsOf(const juce::AudioBuffer<float>& buf)
{
    const int N = buf.getNumChannels() * buf.getNumSamples();
    if (N == 0) return 0.f;
    double sum = 0.0;
    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        for (int i = 0; i < buf.getNumSamples(); ++i)
        {
            double s = buf.getSample(ch, i);
            sum += s * s;
        }
    return (float)std::sqrt(sum / N);
}

static inline bool allFinite(const juce::AudioBuffer<float>& buf)
{
    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        for (int i = 0; i < buf.getNumSamples(); ++i)
            if (!std::isfinite(buf.getSample(ch, i)))
                return false;
    return true;
}

static inline bool allBounded(const juce::AudioBuffer<float>& buf, float maxAbs = 2.f)
{
    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        for (int i = 0; i < buf.getNumSamples(); ++i)
            if (std::abs(buf.getSample(ch, i)) > maxAbs)
                return false;
    return true;
}

// Push N blocks of silence through a processor to drain internal delay buffers
static inline void flushWithSilence(juce::AudioProcessor& proc, int numBlocks = 20, int blockSize = 512)
{
    juce::MidiBuffer midi;
    for (int n = 0; n < numBlocks; ++n)
    {
        auto buf = makeSilent(2, blockSize);
        proc.processBlock(buf, midi);
    }
}

// Set a parameter by its native (denormalised) value
static inline void setParam(juce::AudioProcessorValueTreeState& apvts,
                             const juce::String& id, float nativeValue)
{
    if (auto* p = dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(id)))
        p->setValueNotifyingHost(p->convertTo0to1(nativeValue));
}

// Get a parameter's current native value
static inline float getParam(juce::AudioProcessorValueTreeState& apvts, const juce::String& id)
{
    return *apvts.getRawParameterValue(id);
}
