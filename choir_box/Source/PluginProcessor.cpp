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
    : fft (std::make_unique<juce::dsp::FFT> (kFftOrder))
{
    inFifo            .resize ((size_t)kHopSize, 0.f);
    outAccum          .resize ((size_t)kFftSize * 2, 0.f);
    outFifo           .resize ((size_t)kHopSize, 0.f);
    inputHistory      .resize ((size_t)kFftSize, 0.f);
    fftBuffer         .resize ((size_t)kFftSize * 2, 0.f);
    lastAnalysisPhase .resize ((size_t)kNumBins, 0.f);
    synthPhase        .resize ((size_t)kNumBins, 0.f);
    window            .resize ((size_t)kFftSize, 0.f);
    mag               .resize ((size_t)kNumBins, 0.f);
    trueFreq          .resize ((size_t)kNumBins, 0.f);
    outMag            .resize ((size_t)kNumBins, 0.f);
    outFreq           .resize ((size_t)kNumBins, 0.f);

    // Hann window — length kFftSize+1, use only first kFftSize samples.
    // The +1 is required for correct overlap-add cancellation (JUCE convention).
    // NOT sqrt-Hann — plain Hann applied at both analysis and synthesis stages.
    // With plain Hann twice and 4x overlap, OLA gain = 1.5, corrected by norm = 2/3.
    for (int i = 0; i < kFftSize; ++i)
    {
        window[(size_t)i] = 0.5f * (1.f - std::cos (2.f * juce::MathConstants<float>::pi
                                                      * (float)i / (float)kFftSize)); // note: kFftSize not kFftSize-1
    }
}

void PitchShifter::prepare (double sr)
{
    sampleRate = sr;
    reset();
}

void PitchShifter::reset()
{
    std::fill (inFifo.begin(),            inFifo.end(),            0.f);
    std::fill (outAccum.begin(),          outAccum.end(),          0.f);
    std::fill (outFifo.begin(),           outFifo.end(),           0.f);
    std::fill (inputHistory.begin(),      inputHistory.end(),      0.f);
    std::fill (fftBuffer.begin(),         fftBuffer.end(),         0.f);
    std::fill (lastAnalysisPhase.begin(), lastAnalysisPhase.end(), 0.f);
    std::fill (synthPhase.begin(),        synthPhase.end(),        0.f);
    fifoIdx   = 0;
    outIdx    = 0;
    historyIdx = 0;
}

void PitchShifter::setPitchRatio (float ratio) { pitchRatio = ratio; }

float PitchShifter::processSample (float in)
{
    // Accumulate input into hop-sized fifo
    inFifo[(size_t)fifoIdx] = in;
    ++fifoIdx;

    if (fifoIdx >= kHopSize)
    {
        fifoIdx = 0;

        // Copy hop into history ring buffer
        for (int i = 0; i < kHopSize; ++i)
        {
            inputHistory[(size_t)(historyIdx & (kFftSize - 1))] = inFifo[(size_t)i];
            ++historyIdx;
        }

        processFrame();

        // Shift output accum down by one hop, expose next hop in outFifo
        for (int i = 0; i < kHopSize; ++i)
            outFifo[(size_t)i] = outAccum[(size_t)i];

        // Shift accumulator
        std::copy (outAccum.begin() + kHopSize, outAccum.end(), outAccum.begin());
        std::fill (outAccum.end() - kHopSize, outAccum.end(), 0.f);

        outIdx = 0;
    }

    // Read from current output hop
    float out = 0.f;
    if (outIdx < kHopSize)
        out = outFifo[(size_t)outIdx];
    ++outIdx;

    return out;
}

void PitchShifter::processFrame()
{
    const float twoPi = 2.f * juce::MathConstants<float>::pi;

    // ── 1. Build windowed analysis frame from input history ───────────────────
    // Copy the last kFftSize samples from the ring buffer into the FFT work
    // buffer (first half), zero the second half (required by JUCE real FFT).
    for (int i = 0; i < kFftSize; ++i)
    {
        int srcIdx = (historyIdx - kFftSize + i) & (kFftSize - 1);
        fftBuffer[(size_t)i]              = inputHistory[(size_t)srcIdx] * window[(size_t)i];
        fftBuffer[(size_t)(i + kFftSize)] = 0.f;
    }

    // ── 2. Forward FFT ────────────────────────────────────────────────────────
    // JUCE real-only forward transform with onlyCalculateNonNegativeFrequencies=true.
    // Output: plain interleaved re/im pairs, bin k at [k*2] and [k*2+1].
    // Only the first (kFftSize/2 + 1) complex pairs are valid.
    fft->performRealOnlyForwardTransform (fftBuffer.data(), true);

    // ── 3. Analysis: magnitude + true instantaneous frequency ────────────────
    const float expectedPhaseInc = twoPi * (float)kHopSize / (float)kFftSize;

    for (int k = 0; k < kNumBins; ++k)
    {
        float re = fftBuffer[(size_t)(k * 2)];
        float im = fftBuffer[(size_t)(k * 2 + 1)];

        mag[(size_t)k] = std::sqrt (re * re + im * im);

        float phase = std::atan2 (im, re);

        // Phase deviation from expected advance for this bin
        float delta = phase - lastAnalysisPhase[(size_t)k] - (float)k * expectedPhaseInc;
        lastAnalysisPhase[(size_t)k] = phase;

        // Wrap to -pi..pi
        delta -= twoPi * std::round (delta / twoPi);

        // True frequency expressed in bin units
        trueFreq[(size_t)k] = (float)k + delta / expectedPhaseInc;
    }

    // ── 4. Pitch shift — scatter bins to new positions ────────────────────────
    std::fill (outMag.begin(),  outMag.end(),  0.f);
    std::fill (outFreq.begin(), outFreq.end(), 0.f);

    for (int k = 0; k < kNumBins; ++k)
    {
        int dest = (int)std::round ((float)k * pitchRatio);
        if (dest >= 0 && dest < kNumBins)
        {
            outMag[(size_t)dest]  += mag[(size_t)k];
            outFreq[(size_t)dest]  = trueFreq[(size_t)k] * pitchRatio;
        }
    }

    // ── 5. Synthesis: accumulate phase, reconstruct complex bins ─────────────
    for (int k = 0; k < kNumBins; ++k)
    {
        synthPhase[(size_t)k] += outFreq[(size_t)k] * expectedPhaseInc;
        float s = synthPhase[(size_t)k];
        fftBuffer[(size_t)(k * 2)]     = outMag[(size_t)k] * std::cos (s);
        fftBuffer[(size_t)(k * 2 + 1)] = outMag[(size_t)k] * std::sin (s);
    }
    // Zero the negative-frequency half that JUCE doesn't use but may inspect
    for (int k = kNumBins; k < kFftSize; ++k)
    {
        fftBuffer[(size_t)(k * 2)]     = 0.f;
        fftBuffer[(size_t)(k * 2 + 1)] = 0.f;
    }

    // ── 6. Inverse FFT ────────────────────────────────────────────────────────
    fft->performRealOnlyInverseTransform (fftBuffer.data());

    // ── 7. Overlap-add ────────────────────────────────────────────────────────
    // JUCE IFFT normalises by 1/N internally. Plain Hann applied twice with
    // 4x overlap gives OLA gain of 1.5, so compensation factor = 2/3.
    const float norm = 2.f / 3.f;

    for (int i = 0; i < kFftSize; ++i)
        outAccum[(size_t)i] += fftBuffer[(size_t)i] * window[(size_t)i] * norm;
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

    // Report latency: one FFT window so host can compensate
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
        const float satL     = std::tanh (inL * satDrive) * satComp;
        const float satR     = std::tanh (inR * satDrive) * satComp;
        const float clipThresh = 1.f - crush * 0.85f;
        const float clipL    = juce::jlimit (-clipThresh, clipThresh, satL);
        const float clipR    = juce::jlimit (-clipThresh, clipThresh, satR);
        const float distL    = inL + distMix * (clipL - inL);
        const float distR    = inR + distMix * (clipR - inR);

        // ── Voices ────────────────────────────────────────────────────────────
        float outL = distL * dryLvl;
        float outR = distR * dryLvl;

        for (int v = 0; v < numVoices; ++v)
        {
            const float t = (numVoices > 1)
                            ? (float)v / (float)(numVoices - 1)
                            : 0.5f;

            const float upPan    = 0.5f + t * 0.5f;
            const float downPan  = 0.5f - t * 0.5f;
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

juce::AudioProcessorEditor* ChoirBoxProcessor::createEditor()
{
    return new ChoirBoxEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ChoirBoxProcessor();
}