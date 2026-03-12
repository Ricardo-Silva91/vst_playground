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

// ── PitchShifter implementation ───────────────────────────────────────────────
PitchShifter::PitchShifter()
{
    fft = std::make_unique<juce::dsp::FFT> (11);  // 2^11 = 2048
    inBuf      .resize (kFftSize * 4, 0.f);
    outBuf     .resize (kFftSize * 4, 0.f);
    lastPhase  .resize (kFftSize / 2 + 1, 0.f);
    synthPhase .resize (kFftSize / 2 + 1, 0.f);
    window     .resize (kFftSize, 0.f);
    timeDomain .resize (kFftSize * 2, 0.f);
    freqDomain .resize (kFftSize / 2 + 1);

    // Hann window
    for (int i = 0; i < kFftSize; ++i)
        window[(size_t)i] = 0.5f * (1.f - std::cos (2.f * juce::MathConstants<float>::pi * (float)i / (float)(kFftSize - 1)));

    outputLatency = kFftSize;
}

void PitchShifter::prepare (double sr)
{
    sampleRate = sr;
    reset();
}

void PitchShifter::reset()
{
    std::fill (inBuf.begin(),     inBuf.end(),     0.f);
    std::fill (outBuf.begin(),    outBuf.end(),     0.f);
    std::fill (lastPhase.begin(), lastPhase.end(),  0.f);
    std::fill (synthPhase.begin(),synthPhase.end(), 0.f);
    std::fill (timeDomain.begin(),timeDomain.end(), 0.f);
    inWritePos  = 0;
    outReadPos  = 0;
    outWritePos = kFftSize; // prime with latency offset
    inputFill   = 0;
}

void PitchShifter::setPitchRatio (float ratio)
{
    pitchRatio = ratio;
}

float PitchShifter::processSample (float in)
{
    const int bufMask = (int)inBuf.size() - 1;

    inBuf[(size_t)(inWritePos & bufMask)] = in;
    inWritePos++;
    inputFill++;

    // Process a new frame every hopSize samples
    if (inputFill >= kHopSize)
    {
        inputFill = 0;
        processFrame();
    }

    float out = outBuf[(size_t)(outReadPos & bufMask)];
    outBuf[(size_t)(outReadPos & bufMask)] = 0.f;
    outReadPos++;
    return out;
}

void PitchShifter::processFrame()
{
    const int bufMask  = (int)inBuf.size() - 1;
    const int outMask  = (int)outBuf.size() - 1;
    const int numBins  = kFftSize / 2 + 1;

    // ── 1. Fill time-domain buffer with windowed input ────────────────────────
    int readStart = inWritePos - kFftSize;
    for (int i = 0; i < kFftSize; ++i)
    {
        timeDomain[(size_t)i]            = inBuf[(size_t)((readStart + i) & bufMask)] * window[(size_t)i];
        timeDomain[(size_t)(i + kFftSize)] = 0.f;  // zero-pad for FFT
    }

    // ── 2. Forward FFT ────────────────────────────────────────────────────────
    fft->performRealOnlyForwardTransform (timeDomain.data(), true);

    // Pack into complex bins
    for (int b = 0; b < numBins; ++b)
        freqDomain[(size_t)b] = { timeDomain[(size_t)(b * 2)], timeDomain[(size_t)(b * 2 + 1)] };

    // ── 3. Phase vocoder — analyse phase, synthesise at new pitch ─────────────
    const float expectedPhaseDiff = 2.f * juce::MathConstants<float>::pi
                                    * kHopSize / kFftSize;

    std::vector<float> mag   (numBins);
    std::vector<float> phase (numBins);

    for (int b = 0; b < numBins; ++b)
    {
        float re = freqDomain[(size_t)b].real();
        float im = freqDomain[(size_t)b].imag();
        mag[(size_t)b]   = std::sqrt (re*re + im*im);
        float p  = std::atan2 (im, re);

        // True frequency deviation from expected
        float delta = p - lastPhase[(size_t)b] - (float)b * expectedPhaseDiff;
        // Wrap to -pi..pi
        delta -= 2.f * juce::MathConstants<float>::pi
                 * std::round (delta / (2.f * juce::MathConstants<float>::pi));

        lastPhase[(size_t)b] = p;
        phase[(size_t)b] = delta / expectedPhaseDiff + (float)b;  // true bin frequency
    }

    // Resample magnitude/phase to shifted bin positions
    std::vector<float> outMag   (numBins, 0.f);
    std::vector<float> outPhase (numBins, 0.f);

    for (int b = 0; b < numBins; ++b)
    {
        int shifted = (int)std::round ((float)b * pitchRatio);
        if (shifted >= 0 && shifted < numBins)
        {
            outMag[(size_t)shifted]   += mag[(size_t)b];
            outPhase[(size_t)shifted]  = phase[(size_t)b] * pitchRatio;
        }
    }

    // Accumulate synthesis phase
    for (int b = 0; b < numBins; ++b)
    {
        synthPhase[(size_t)b] += outPhase[(size_t)b] * expectedPhaseDiff;
        freqDomain[(size_t)b]  = std::polar (outMag[(size_t)b], synthPhase[(size_t)b]);
    }

    // ── 4. Inverse FFT ────────────────────────────────────────────────────────
    for (int b = 0; b < numBins; ++b)
    {
        timeDomain[(size_t)(b * 2)]     = freqDomain[(size_t)b].real();
        timeDomain[(size_t)(b * 2 + 1)] = freqDomain[(size_t)b].imag();
    }
    fft->performRealOnlyInverseTransform (timeDomain.data());

    // ── 5. Overlap-add to output buffer ───────────────────────────────────────
    const float scale = 1.f / (float)(kOverlap * kHopSize) * 2.f;
    for (int i = 0; i < kFftSize; ++i)
    {
        outBuf[(size_t)((outWritePos + i) & outMask)] += timeDomain[(size_t)i] * window[(size_t)i] * scale;
    }
    outWritePos += kHopSize;
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

    setLatencySamples (PitchShifter::kFftSize);
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
    const float upSemi   = apvts.getRawParameterValue (kUpSemitones)  ->load();
    const float downSemi = apvts.getRawParameterValue (kDownSemitones)->load();
    const int   numVoices= juce::jlimit (1, kMaxVoices,
                               (int)std::round (apvts.getRawParameterValue (kVoices)->load()));
    const float detune   = apvts.getRawParameterValue (kDetune)       ->load();
    const float dryLvl   = apvts.getRawParameterValue (kDryLevel)     ->load();
    const float upLvl    = apvts.getRawParameterValue (kUpLevel)      ->load();
    const float downLvl  = apvts.getRawParameterValue (kDownLevel)    ->load();
    const float sat      = apvts.getRawParameterValue (kSaturation)   ->load();
    const float crush    = apvts.getRawParameterValue (kCrush)        ->load();
    const float distMix  = apvts.getRawParameterValue (kDistMix)      ->load();
    const float masterOut= apvts.getRawParameterValue (kMasterOut)    ->load();

    // ── Update pitch ratios for each voice ────────────────────────────────────
    // Detune spreads voices evenly around the target semitone, in cents.
    // centre-of-mass always stays at target interval.
    for (int v = 0; v < kMaxVoices; ++v)
    {
        // spread offset in semitones: for v voices, spread = detune/100 semitones total
        float spreadSt = (detune / 100.f);  // total spread in semitones
        float offset   = (numVoices > 1)
                         ? spreadSt * ((float)v / (float)(numVoices - 1) - 0.5f)
                         : 0.f;

        float upRatio   = std::pow (2.f, (upSemi   + offset) / 12.f);
        float downRatio = std::pow (2.f, (downSemi + offset) / 12.f);

        upShifterL[(size_t)v].setPitchRatio (upRatio);
        upShifterR[(size_t)v].setPitchRatio (upRatio);
        downShifterL[(size_t)v].setPitchRatio (downRatio);
        downShifterR[(size_t)v].setPitchRatio (downRatio);
    }

    float* L = buffer.getWritePointer (0);
    float* R = (numChannels > 1) ? buffer.getWritePointer (1) : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        float inL = L[i];
        float inR = (R != nullptr) ? R[i] : inL;

        // ── Distortion stage ──────────────────────────────────────────────────
        // Soft saturation — tanh
        float satL = std::tanh (inL * (1.f + sat * 4.f)) / (1.f + sat * 0.5f);
        float satR = std::tanh (inR * (1.f + sat * 4.f)) / (1.f + sat * 0.5f);

        // Hard clip — increasingly aggressive
        float clipThresh = 1.f - crush * 0.85f;
        float clipL = juce::jlimit (-clipThresh, clipThresh, satL);
        float clipR = juce::jlimit (-clipThresh, clipThresh, satR);

        // Blend distorted with clean input
        float distL = inL + distMix * (clipL - inL);
        float distR = inR + distMix * (clipR - inR);

        // ── Pitch shifting ────────────────────────────────────────────────────
        float outL = distL * dryLvl;
        float outR = distR * dryLvl;

        for (int v = 0; v < numVoices; ++v)
        {
            // Pan: up voices spread center → hard right; down voices center → hard left
            float upPan, downPan;
            if (numVoices == 1)
            {
                upPan   = 0.65f;   // slight right
                downPan = 0.35f;   // slight left
            }
            else
            {
                // v=0 → near centre, v=numVoices-1 → hard side
                float t = (float)v / (float)(numVoices - 1);
                upPan   = 0.5f + t * 0.5f;   // 0.5 → 1.0
                downPan = 0.5f - t * 0.5f;   // 0.5 → 0.0
            }

            float upL_shifted   = upShifterL[(size_t)v].processSample (distL);
            float upR_shifted   = upShifterR[(size_t)v].processSample (distR);
            float downL_shifted = downShifterL[(size_t)v].processSample (distL);
            float downR_shifted = downShifterR[(size_t)v].processSample (distR);

            // Equal-power pan law
            float upPanR   = std::sin (upPan   * juce::MathConstants<float>::halfPi);
            float upPanL   = std::cos (upPan   * juce::MathConstants<float>::halfPi);
            float downPanL = std::cos (downPan * juce::MathConstants<float>::halfPi);
            float downPanR = std::sin (downPan * juce::MathConstants<float>::halfPi);

            float voiceScale = 1.f / (float)numVoices;

            outL += upLvl   * voiceScale * upPanL   * (upL_shifted   + upR_shifted)   * 0.5f;
            outR += upLvl   * voiceScale * upPanR   * (upL_shifted   + upR_shifted)   * 0.5f;
            outL += downLvl * voiceScale * downPanL * (downL_shifted + downR_shifted) * 0.5f;
            outR += downLvl * voiceScale * downPanR * (downL_shifted + downR_shifted) * 0.5f;
        }

        // Master output
        L[i] = outL * masterOut;
        if (R != nullptr) R[i] = outR * masterOut;
    }
}

// ── Preset system ─────────────────────────────────────────────────────────────
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

// ── State save/load ───────────────────────────────────────────────────────────
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