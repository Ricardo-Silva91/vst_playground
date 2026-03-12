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
    buf.fill (0.f);
    // Hann window for the crossfade grain
    for (int i = 0; i < kGrainSize; ++i)
        window[(size_t)i] = 0.5f * (1.f - std::cos (
            2.f * juce::MathConstants<float>::pi * (float)i / (float)(kGrainSize - 1)));
}

void PitchShifter::prepare (double /*sampleRate*/)
{
    reset();
}

void PitchShifter::reset()
{
    buf.fill (0.f);
    writePos  = 0;
    readPos   = 0.f;
    readPos2  = (float)(kBufSize / 2);  // second grain offset by half buffer
    grainPos  = 0;
    useSecond = false;
}

void PitchShifter::setPitchRatio (float ratio)
{
    pitchRatio = ratio;
}

float PitchShifter::processSample (float in)
{
    // Write input into circular buffer
    buf[(size_t)(writePos & (kBufSize - 1))] = in;

    // The read pointer advances at pitchRatio relative to the write pointer.
    // Speed > 1 = pitch up, speed < 1 = pitch down.
    // We read at a fractional position using linear interpolation.

    auto readLinear = [&](float pos) -> float
    {
        int   i0 = (int)pos & (kBufSize - 1);
        int   i1 = (i0 + 1) & (kBufSize - 1);
        float fr = pos - std::floor (pos);
        return buf[(size_t)i0] * (1.f - fr) + buf[(size_t)i1] * fr;
    };

    // Distance between read and write pointers (read pointer lags behind)
    // We keep read pointer approximately kGrainSize samples behind write.
    float targetLag = (float)(kGrainSize);
    float readPosAbs  = (float)writePos - targetLag;
    // Let read pointer drift according to pitch ratio instead of tracking exactly:
    // advance read pointer by pitchRatio each sample
    readPos  += pitchRatio;
    readPos2 += pitchRatio;

    // Wrap read pointers within buffer
    while (readPos  >= (float)kBufSize) readPos  -= (float)kBufSize;
    while (readPos  <  0.f)             readPos  += (float)kBufSize;
    while (readPos2 >= (float)kBufSize) readPos2 -= (float)kBufSize;
    while (readPos2 <  0.f)             readPos2 += (float)kBufSize;

    float s1 = readLinear (readPos);
    float s2 = readLinear (readPos2);

    // Crossfade between grain 1 and grain 2 using the Hann window position
    float w1 = window[(size_t)(grainPos % kGrainSize)];
    float w2 = window[(size_t)((grainPos + kGrainSize / 2) % kGrainSize)];
    float out = s1 * w1 + s2 * w2;

    grainPos = (grainPos + 1) % kGrainSize;

    writePos++;
    (void)readPosAbs;
    return out;
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

    // ── Read parameters ───────────────────────────────────────────────────────
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

    // ── Update pitch ratios ───────────────────────────────────────────────────
    // Voices are spread symmetrically around the target semitone.
    // detune controls total spread in semitones (0-1 semitone range).
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
        float inL = L[i];
        float inR = (R != nullptr) ? R[i] : inL;

        // ── Distortion ────────────────────────────────────────────────────────
        float satDrive = 1.f + sat * 4.f;
        float satComp  = 1.f / (1.f + sat * 0.5f);
        float satL = std::tanh (inL * satDrive) * satComp;
        float satR = std::tanh (inR * satDrive) * satComp;

        float clipThresh = 1.f - crush * 0.85f;
        float clipL = juce::jlimit (-clipThresh, clipThresh, satL);
        float clipR = juce::jlimit (-clipThresh, clipThresh, satR);

        float distL = inL + distMix * (clipL - inL);
        float distR = inR + distMix * (clipR - inR);

        // ── Mix voices ────────────────────────────────────────────────────────
        float outL = distL * dryLvl;
        float outR = distR * dryLvl;

        for (int v = 0; v < numVoices; ++v)
        {
            // Pan: up voices spread centre→right, down voices centre→left
            float t = (numVoices > 1) ? (float)v / (float)(numVoices - 1) : 0.5f;
            float upPan   = 0.5f + t * 0.5f;   // 0.5 → 1.0
            float downPan = 0.5f - t * 0.5f;   // 0.5 → 0.0

            float upPanR   = std::sin (upPan   * juce::MathConstants<float>::halfPi);
            float upPanL   = std::cos (upPan   * juce::MathConstants<float>::halfPi);
            float downPanL = std::cos (downPan * juce::MathConstants<float>::halfPi);
            float downPanR = std::sin (downPan * juce::MathConstants<float>::halfPi);

            float upL   = upShifterL[(size_t)v].processSample (distL);
            float upR   = upShifterR[(size_t)v].processSample (distR);
            float dnL   = downShifterL[(size_t)v].processSample (distL);
            float dnR   = downShifterR[(size_t)v].processSample (distR);

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