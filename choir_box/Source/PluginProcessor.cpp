#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

// ── Parameter IDs ─────────────────────────────────────────────────────────────
static const juce::String kUpSemitones   = "upSemitones";
static const juce::String kDownSemitones = "downSemitones";
static const juce::String kVoices        = "voices";
static const juce::String kDetune        = "detune";
static const juce::String kDryLevel      = "dryLevel";
static const juce::String kUpLevel       = "upLevel";
static const juce::String kDownLevel     = "downLevel";
static const juce::String kSaturation    = "saturation";
static const juce::String kCrush         = "crush";
static const juce::String kDistMix       = "distMix";
static const juce::String kMasterOut     = "masterOut";

// ── PitchShifter ──────────────────────────────────────────────────────────────
PitchShifter::PitchShifter()
{
    // Build Hann window
    for (int i = 0; i < kGrainSize; ++i)
        hann[(size_t)i] = 0.5f * (1.f - std::cos (
            2.f * juce::MathConstants<float>::pi
            * (float)i / (float)(kGrainSize - 1)));
}

void PitchShifter::prepare (double /*sampleRate*/)
{
    reset();
}

void PitchShifter::reset()
{
    buf.fill (0.f);
    writePos = 0;

    // Initialise grains so they are exactly half a grain apart in their
    // envelope phases — grain 0 starts at the beginning of its window,
    // grain 1 starts at the halfway point.
    grains[0].readPos = 0.f;
    grains[0].envPos  = 0;
    grains[1].readPos = (float)(kGrainSize / 2);
    grains[1].envPos  = kGrainSize / 2;
}

void PitchShifter::setPitchRatio (float ratio)
{
    pitchRatio = ratio;
}

float PitchShifter::readAt (float pos) const
{
    // Wrap into buffer
    while (pos >= (float)kBufSize) pos -= (float)kBufSize;
    while (pos <  0.f)             pos += (float)kBufSize;

    int   i0 = (int)pos & (kBufSize - 1);
    int   i1 = (i0 + 1) & (kBufSize - 1);
    float fr = pos - std::floor (pos);
    return buf[(size_t)i0] * (1.f - fr) + buf[(size_t)i1] * fr;
}

float PitchShifter::processSample (float in)
{
    // Write new sample
    buf[(size_t)(writePos & (kBufSize - 1))] = in;

    float out = 0.f;

    for (int g = 0; g < 2; ++g)
    {
        Grain& gr = grains[g];

        // Apply Hann envelope and accumulate
        float env = hann[(size_t)gr.envPos];
        out += readAt (gr.readPos) * env;

        // Advance read position by pitchRatio (fractional sample stepping)
        gr.readPos += pitchRatio;

        // Advance envelope position
        gr.envPos++;

        // When this grain completes its window, reset it:
        // - restart the envelope from 0
        // - jump the read position forward by kGrainSize relative to
        //   the current write position so it stays anchored to fresh audio.
        //   The offset between read and write gives us the effective delay;
        //   we keep it at kGrainSize samples so the grain always has
        //   a full window of valid input behind it.
        if (gr.envPos >= kGrainSize)
        {
            gr.envPos  = 0;
            gr.readPos = (float)(writePos - kGrainSize);
        }
    }

    writePos++;

    // Scale: two grains with overlapping Hann windows sum to ~1.0 on average
    // but we apply a small correction factor to keep unity gain
    return out * 1.f;
}

// ── ChoirBoxProcessor ─────────────────────────────────────────────────────────
ChoirBoxProcessor::ChoirBoxProcessor()
    : AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
}

ChoirBoxProcessor::~ChoirBoxProcessor() {}

juce::AudioProcessorValueTreeState::ParameterLayout
ChoirBoxProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (kUpSemitones,   "Up Semitones",   -24.f, 24.f,  7.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (kDownSemitones, "Down Semitones", -24.f, 24.f, -7.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (kVoices,        "Voices",          1.f,  4.f,   1.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (kDetune,        "Detune",          0.f,  100.f, 20.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (kDryLevel,      "Dry Level",       0.f,  1.f,   1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (kUpLevel,       "Up Level",        0.f,  1.f,   0.7f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (kDownLevel,     "Down Level",      0.f,  1.f,   0.7f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (kSaturation,    "Saturation",      0.f,  1.f,   0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (kCrush,         "Crush",           0.f,  1.f,   0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (kDistMix,       "Dist Mix",        0.f,  1.f,   0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (kMasterOut,     "Master Out",      0.f,  2.f,   1.0f));

    return { params.begin(), params.end() };
}

void ChoirBoxProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate;

    for (int v = 0; v < kMaxVoices; ++v)
    {
        upShifterL[(size_t)v].prepare (sampleRate);
        upShifterR[(size_t)v].prepare (sampleRate);
        downShifterL[(size_t)v].prepare (sampleRate);
        downShifterR[(size_t)v].prepare (sampleRate);
    }

    applyPreset (currentPreset);
}

void ChoirBoxProcessor::releaseResources() {}

void ChoirBoxProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                       juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    if (numChannels < 1 || numSamples < 1) return;

    // ── Parameters ────────────────────────────────────────────────────────────
    const float upSemi    = apvts.getRawParameterValue (kUpSemitones)  ->load();
    const float downSemi  = apvts.getRawParameterValue (kDownSemitones)->load();
    const int   numVoices = juce::jlimit (1, kMaxVoices,
                                (int)std::round (apvts.getRawParameterValue (kVoices)->load()));
    const float detune    = apvts.getRawParameterValue (kDetune)       ->load();
    const float dryLvl    = apvts.getRawParameterValue (kDryLevel)     ->load();
    const float upLvl     = apvts.getRawParameterValue (kUpLevel)      ->load();
    const float downLvl   = apvts.getRawParameterValue (kDownLevel)    ->load();
    const float sat       = apvts.getRawParameterValue (kSaturation)   ->load();
    const float crush     = apvts.getRawParameterValue (kCrush)        ->load();
    const float distMix   = apvts.getRawParameterValue (kDistMix)      ->load();
    const float masterOut = apvts.getRawParameterValue (kMasterOut)    ->load();

    // ── Pitch ratios ──────────────────────────────────────────────────────────
    for (int v = 0; v < kMaxVoices; ++v)
    {
        float offset = 0.f;
        if (numVoices > 1)
        {
            float spreadSt = detune / 100.f;
            offset = spreadSt * ((float)v / (float)(numVoices - 1) - 0.5f);
        }

        float upRatio   = std::pow (2.f, (upSemi   + offset) / 12.f);
        float downRatio = std::pow (2.f, (downSemi + offset) / 12.f);

        upShifterL[(size_t)v].setPitchRatio (upRatio);
        upShifterR[(size_t)v].setPitchRatio (upRatio);
        downShifterL[(size_t)v].setPitchRatio (downRatio);
        downShifterR[(size_t)v].setPitchRatio (downRatio);
    }

    float* L = buffer.getWritePointer (0);
    float* R = (numChannels > 1) ? buffer.getWritePointer (1) : nullptr;

    const float voiceScale = 1.f / (float)numVoices;

    for (int i = 0; i < numSamples; ++i)
    {
        const float inL = L[i];
        const float inR = (R != nullptr) ? R[i] : inL;

        // ── Distortion ────────────────────────────────────────────────────────
        const float satDrive = 1.f + sat * 4.f;
        const float satComp  = 1.f / (1.f + sat * 0.5f);
        float satL = std::tanh (inL * satDrive) * satComp;
        float satR = std::tanh (inR * satDrive) * satComp;

        const float clipThresh = 1.f - crush * 0.85f;
        const float clipL = juce::jlimit (-clipThresh, clipThresh, satL);
        const float clipR = juce::jlimit (-clipThresh, clipThresh, satR);

        const float distL = inL + distMix * (clipL - inL);
        const float distR = inR + distMix * (clipR - inR);

        // ── Voices ────────────────────────────────────────────────────────────
        float outL = distL * dryLvl;
        float outR = distR * dryLvl;

        for (int v = 0; v < numVoices; ++v)
        {
            // Equal-power pan — up voices spread right, down voices spread left
            const float t = (numVoices > 1)
                            ? (float)v / (float)(numVoices - 1)
                            : 0.5f;

            const float upPan   = 0.5f + t * 0.5f;
            const float downPan = 0.5f - t * 0.5f;

            const float upPanR   = std::sin (upPan   * juce::MathConstants<float>::halfPi);
            const float upPanL   = std::cos (upPan   * juce::MathConstants<float>::halfPi);
            const float downPanL = std::cos (downPan * juce::MathConstants<float>::halfPi);
            const float downPanR = std::sin (downPan * juce::MathConstants<float>::halfPi);

            const float upL  = upShifterL[(size_t)v].processSample (distL);
            const float upR  = upShifterR[(size_t)v].processSample (distR);
            const float dnL  = downShifterL[(size_t)v].processSample (distL);
            const float dnR  = downShifterR[(size_t)v].processSample (distR);

            outL += upLvl   * voiceScale * upPanL   * upL;
            outR += upLvl   * voiceScale * upPanR   * upR;
            outL += downLvl * voiceScale * downPanL * dnL;
            outR += downLvl * voiceScale * downPanR * dnR;
        }

        L[i] = outL * masterOut;
        if (R != nullptr) R[i] = outR * masterOut;
    }
}

// ── Presets ───────────────────────────────────────────────────────────────────
void ChoirBoxProcessor::applyPreset (int index)
{
    if (index < 0 || index >= kNumPresets) return;
    currentPreset = index;
    const auto& p = kPresets[index];

    auto set = [&](const juce::String& id, float val)
    {
        auto* param = apvts.getParameter (id);
        if (param) param->setValueNotifyingHost (param->convertTo0to1 (val));
    };

    set (kUpSemitones,   p.upSemitones);
    set (kDownSemitones, p.downSemitones);
    set (kVoices,        p.voices);
    set (kDetune,        p.detune);
    set (kDryLevel,      p.dryLevel);
    set (kUpLevel,       p.upLevel);
    set (kDownLevel,     p.downLevel);
    set (kSaturation,    p.saturation);
    set (kCrush,         p.crush);
    set (kDistMix,       p.distMix);
    set (kMasterOut,     p.masterOut);
}

void ChoirBoxProcessor::setCurrentProgram (int index) { applyPreset (index); }

const juce::String ChoirBoxProcessor::getProgramName (int index)
{
    if (index >= 0 && index < kNumPresets) return kPresets[index].name;
    return "Unknown";
}

// ── State ─────────────────────────────────────────────────────────────────────
void ChoirBoxProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    xml->setAttribute ("currentPreset", currentPreset);
    copyXmlToBinary (*xml, destData);
}

void ChoirBoxProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
    {
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
        currentPreset = xml->getIntAttribute ("currentPreset", 0);
    }
}

// ── Editor + factory ──────────────────────────────────────────────────────────
juce::AudioProcessorEditor* ChoirBoxProcessor::createEditor()
{
    return new ChoirBoxEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ChoirBoxProcessor();
}