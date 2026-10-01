#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "SharedProcessorUtils.h"

// ── Parameter IDs ──────────────────────────────────────────────────────────────
static const juce::String kBitDepth       = "bitDepth";
static const juce::String kSampleRateDiv  = "sampleRateDiv";
static const juce::String kDrive          = "drive";
static const juce::String kOutputGain     = "outputGain";
static const juce::String kNoiseAmount    = "noiseAmount";
static const juce::String kCrackleRate    = "crackleRate";
static const juce::String kLpfCutoff      = "lpfCutoff";
static const juce::String kHpfCutoff      = "hpfCutoff";
static const juce::String kCompThreshold  = "compThreshold";
static const juce::String kCompRatio      = "compRatio";
static const juce::String kCompAttack     = "compAttack";
static const juce::String kCompRelease    = "compRelease";
static const juce::String kCompMakeup     = "compMakeup";
static const juce::String kReverbRoom     = "reverbRoom";
static const juce::String kReverbWet      = "reverbWet";
static const juce::String kReverbDamping  = "reverbDamping";
static const juce::String kPitchSemitones = "pitchSemitones";
static const juce::String kWowRate        = "wowRate";
static const juce::String kWowDepth       = "wowDepth";
static const juce::String kStereoWidth    = "stereoWidth";
static const juce::String kTransientBoost = "transientBoost";

// ── Constructor ───────────────────────────────────────────────────────────────
DrumSmashProcessor::DrumSmashProcessor()
    : AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    // Start on the first preset; prepareToPlay must not re-apply it, or it
    // would overwrite restored session state and the user's own edits.
    applyPreset (0);
}

DrumSmashProcessor::~DrumSmashProcessor() {}

// ── Parameter layout ──────────────────────────────────────────────────────────
juce::AudioProcessorValueTreeState::ParameterLayout
DrumSmashProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (kBitDepth,      "Bit Depth",       1.f,  16.f, 16.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (kSampleRateDiv, "SR Divide",        1.f,  32.f,  1.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (kDrive,         "Drive",            0.f,   1.f,  0.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (kOutputGain,    "Output Gain",      0.f,   2.f,  1.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (kNoiseAmount,   "Noise Amount",     0.f,   1.f,  0.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (kCrackleRate,   "Crackle Rate",     0.f,   1.f,  0.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (kLpfCutoff,     "LPF Cutoff",     200.f, 22000.f, 22000.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (kHpfCutoff,     "HPF Cutoff",      20.f,  2000.f,  20.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (kCompThreshold, "Comp Threshold", -60.f,    0.f, -18.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (kCompRatio,     "Comp Ratio",       1.f,   20.f,  3.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (kCompAttack,    "Comp Attack ms",   0.1f, 200.f, 10.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (kCompRelease,   "Comp Release ms", 10.f, 2000.f, 150.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (kCompMakeup,    "Comp Makeup dB",   0.f,  24.f,  0.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (kReverbRoom,    "Reverb Room",      0.f,   1.f,  0.3f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (kReverbWet,     "Reverb Wet",       0.f,   1.f,  0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (kReverbDamping, "Reverb Damping",   0.f,   1.f,  0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (kPitchSemitones, "Pitch Semitones", -12.f,  12.f,  0.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (kWowRate,        "Wow/Flutter Hz",   0.f,   8.f,  0.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (kWowDepth,       "Wow/Flutter Cents",0.f,  50.f,  0.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (kStereoWidth,    "Stereo Width",     0.f,   2.f,  1.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (kTransientBoost, "Transient Boost",  0.f,   1.f,  0.f));

    return { params.begin(), params.end() };
}

// ── Preset loading ────────────────────────────────────────────────────────────
void DrumSmashProcessor::applyPreset (int index)
{
    if (index < 0 || index >= kNumPresets) return;
    currentPreset = index;
    const auto& p = kPresets[index];

    SharedProcessorUtils::applyParam (apvts, kBitDepth,       p.bitDepth);
    SharedProcessorUtils::applyParam (apvts, kSampleRateDiv,  p.sampleRateDiv);
    SharedProcessorUtils::applyParam (apvts, kDrive,          p.drive);
    SharedProcessorUtils::applyParam (apvts, kOutputGain,     p.outputGain);
    SharedProcessorUtils::applyParam (apvts, kNoiseAmount,    p.noiseAmount);
    SharedProcessorUtils::applyParam (apvts, kCrackleRate,    p.crackleRate);
    SharedProcessorUtils::applyParam (apvts, kLpfCutoff,      p.lpfCutoff);
    SharedProcessorUtils::applyParam (apvts, kHpfCutoff,      p.hpfCutoff);
    SharedProcessorUtils::applyParam (apvts, kCompThreshold,  p.compThresholdDb);
    SharedProcessorUtils::applyParam (apvts, kCompRatio,      p.compRatio);
    SharedProcessorUtils::applyParam (apvts, kCompAttack,     p.compAttackMs);
    SharedProcessorUtils::applyParam (apvts, kCompRelease,    p.compReleaseMs);
    SharedProcessorUtils::applyParam (apvts, kCompMakeup,     p.compMakeupDb);
    SharedProcessorUtils::applyParam (apvts, kReverbRoom,     p.reverbRoomSize);
    SharedProcessorUtils::applyParam (apvts, kReverbWet,      p.reverbWet);
    SharedProcessorUtils::applyParam (apvts, kReverbDamping,  p.reverbDamping);
    SharedProcessorUtils::applyParam (apvts, kPitchSemitones, p.pitchSemitones);
    SharedProcessorUtils::applyParam (apvts, kWowRate,        p.wowFlutterRate);
    SharedProcessorUtils::applyParam (apvts, kWowDepth,       p.wowFlutterDepth);
    SharedProcessorUtils::applyParam (apvts, kStereoWidth,    p.stereoWidth);
    SharedProcessorUtils::applyParam (apvts, kTransientBoost, p.transientBoost);
}

void DrumSmashProcessor::setCurrentProgram (int index)
{
    applyPreset (index);
}

const juce::String DrumSmashProcessor::getProgramName (int index)
{
    if (index >= 0 && index < kNumPresets)
        return kPresets[index].name;
    return "Unknown";
}

// ── Prepare ───────────────────────────────────────────────────────────────────
void DrumSmashProcessor::rebuildDSP()
{
    if (currentSampleRate <= 0.0) return;

    float lpf = apvts.getRawParameterValue (kLpfCutoff)->load();
    float hpf = apvts.getRawParameterValue (kHpfCutoff)->load();
    lpf = juce::jlimit (200.f, 20000.f, lpf);
    hpf = juce::jlimit (20.f,  2000.f, hpf);

    // Assigning ArrayCoefficients reuses the existing storage, so this is
    // allocation-free on the audio thread; skip it when nothing changed
    if (hpf != lastHpf)
    {
        *hpfFilter.state = juce::dsp::IIR::ArrayCoefficients<float>::makeHighPass (currentSampleRate, hpf, 0.707f);
        lastHpf = hpf;
    }
    if (lpf != lastLpf)
    {
        *lpfFilter.state = juce::dsp::IIR::ArrayCoefficients<float>::makeLowPass (currentSampleRate, lpf, 0.707f);
        lastLpf = lpf;
    }

    float attack  = apvts.getRawParameterValue (kCompAttack) ->load();
    float release = apvts.getRawParameterValue (kCompRelease)->load();
    float thresh  = apvts.getRawParameterValue (kCompThreshold)->load();
    float ratio   = apvts.getRawParameterValue (kCompRatio)->load();

    compressor.setAttack   (attack);
    compressor.setRelease  (release);
    compressor.setThreshold(thresh);
    compressor.setRatio    (ratio);

    juce::Reverb::Parameters rp;
    rp.roomSize   = apvts.getRawParameterValue (kReverbRoom)   ->load();
    rp.wetLevel   = apvts.getRawParameterValue (kReverbWet)    ->load();
    rp.dryLevel   = 1.0f - rp.wetLevel * 0.5f;
    rp.damping    = apvts.getRawParameterValue (kReverbDamping)->load();
    rp.width      = 0.8f;
    reverb.setParameters (rp);
}

void DrumSmashProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    bcPhase = 0.f;
    bcHeldL = 0.f;
    bcHeldR = 0.f;
    wowPhase = 0.f;
    envFast = 0.f;
    envSlow = 0.f;

    pitchWindow   = juce::jmax (4, (int) (0.050 * sampleRate));   // 50 ms grain
    pitchBufL.assign ((size_t) pitchWindow + 2, 0.f);
    pitchBufR.assign ((size_t) pitchWindow + 2, 0.f);
    pitchWritePos = 0;
    pitchPhase    = 0.0;

    const int wowBufSize = (int) (2.0 * kMaxWowDelaySec * sampleRate) + 4;
    wowBufL.assign ((size_t) wowBufSize, 0.f);
    wowBufR.assign ((size_t) wowBufSize, 0.f);
    wowWritePos = 0;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels      = 2;

    hpfFilter.prepare  (spec);
    lpfFilter.prepare  (spec);
    lastHpf = lastLpf = -1.f;
    compressor.prepare (spec);
    reverb.reset();
    reverb.setSampleRate (sampleRate);

    rebuildDSP();
    // Size the filter state for the new coefficients here, not on the audio thread
    hpfFilter.reset();
    lpfFilter.reset();
}

void DrumSmashProcessor::releaseResources() {}

// ── Process ───────────────────────────────────────────────────────────────────
void DrumSmashProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                        juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    if (numChannels < 1) return;

    // ── Read params ──────────────────────────────────────────────────────────
    const float bitDepth      = apvts.getRawParameterValue (kBitDepth)      ->load();
    const float srDiv         = apvts.getRawParameterValue (kSampleRateDiv) ->load();
    const float drive         = apvts.getRawParameterValue (kDrive)         ->load();
    const float outputGain    = apvts.getRawParameterValue (kOutputGain)    ->load();
    const float noiseAmt      = apvts.getRawParameterValue (kNoiseAmount)   ->load();
    const float crackleRate   = apvts.getRawParameterValue (kCrackleRate)   ->load();
    const float compMakeupDb  = apvts.getRawParameterValue (kCompMakeup)    ->load();
    const float pitchSemi     = apvts.getRawParameterValue (kPitchSemitones)->load();
    const float wowRate       = apvts.getRawParameterValue (kWowRate)       ->load();
    const float wowDepth      = apvts.getRawParameterValue (kWowDepth)      ->load();
    const float stereoWidth   = apvts.getRawParameterValue (kStereoWidth)   ->load();
    const float transientBoost= apvts.getRawParameterValue (kTransientBoost)->load();

    const float pitchRatio    = std::pow (2.f, pitchSemi / 12.f);
    const float srDivInt      = std::max (1.f, std::floor (srDiv));
    const float bitLevels     = std::pow (2.f, juce::jlimit (1.f, 16.f, bitDepth));
    const float compMakeupLin = juce::Decibels::decibelsToGain (compMakeupDb);

    // Refresh filter/comp/reverb settings every block (filters only when
    // their cutoff moved; unsmoothed, as before)
    rebuildDSP();

    const bool  pitchActive = std::fabs (pitchRatio - 1.f) > 0.001f;
    const bool  wowActive   = wowDepth > 0.f && wowRate > 0.f;
    const float wowInc      = juce::MathConstants<float>::twoPi * wowRate / (float) currentSampleRate;
    // Vibrato: delay d = A(1 - cos φ) gives a peak pitch deviation of A·ω,
    // so A = (2^(cents/1200) - 1) / ω, capped to the delay buffer
    const float wowAmp = wowActive
        ? juce::jmin ((std::pow (2.f, wowDepth / 1200.f) - 1.f) / wowInc,
                      (float) (kMaxWowDelaySec * currentSampleRate))
        : 0.f;

    // Transient shaper: a peak follower (1 ms attack / 50 ms release) and a
    // 20 ms smoothed copy of it, which lags only when the level jumps
    const float fastAtt = 1.f - std::exp (-1.f / (0.001f * (float) currentSampleRate));
    const float envRel  = 1.f - std::exp (-1.f / (0.050f * (float) currentSampleRate));
    const float slowCoef = 1.f - std::exp (-1.f / (0.020f * (float) currentSampleRate));

    float* L = buffer.getWritePointer (0);
    float* R = (numChannels > 1) ? buffer.getWritePointer (1) : nullptr;

    // ── Per-sample processing ────────────────────────────────────────────────
    for (int i = 0; i < numSamples; ++i)
    {
        float l = L[i];
        float r = (R != nullptr) ? R[i] : l;

        // 1a. Pitch shift: two read taps 180° apart sweep a short delay
        // window at (1 - ratio) samples per sample, equal-power crossfaded
        if (pitchActive)
        {
            pitchBufL[(size_t) pitchWritePos] = l;
            pitchBufR[(size_t) pitchWritePos] = r;

            const int size = (int) pitchBufL.size();
            auto readTap = [&] (const std::vector<float>& b, double delay)
            {
                double pos = (double) pitchWritePos - delay;
                while (pos < 0.0) pos += size;
                const int   i0 = (int) pos;
                const int   i1 = (i0 + 1) % size;
                const float fr = (float) (pos - i0);
                return b[(size_t) i0] + fr * (b[(size_t) i1] - b[(size_t) i0]);
            };

            const double W  = (double) pitchWindow;
            const double d1 = pitchPhase;
            const double d2 = std::fmod (pitchPhase + 0.5 * W, W);
            const float  g1 = std::sin (juce::MathConstants<float>::pi * (float) (d1 / W));
            const float  g2 = std::sin (juce::MathConstants<float>::pi * (float) (d2 / W));

            l = g1 * readTap (pitchBufL, d1) + g2 * readTap (pitchBufL, d2);
            r = g1 * readTap (pitchBufR, d1) + g2 * readTap (pitchBufR, d2);

            pitchWritePos = (pitchWritePos + 1) % size;
            pitchPhase   += 1.0 - (double) pitchRatio;
            if (pitchPhase <  0.0) pitchPhase += W;
            if (pitchPhase >= W)   pitchPhase -= W;
        }

        // 1b. Wow/flutter: LFO-modulated delay, so it bends pitch (not level)
        if (wowActive)
        {
            wowBufL[(size_t) wowWritePos] = l;
            wowBufR[(size_t) wowWritePos] = r;

            const int    size  = (int) wowBufL.size();
            const double delay = (double) wowAmp * (1.0 - std::cos ((double) wowPhase));
            double pos = (double) wowWritePos - delay;
            if (pos < 0.0) pos += size;
            const int   i0 = (int) pos;
            const int   i1 = (i0 + 1) % size;
            const float fr = (float) (pos - i0);
            l = wowBufL[(size_t) i0] + fr * (wowBufL[(size_t) i1] - wowBufL[(size_t) i0]);
            r = wowBufR[(size_t) i0] + fr * (wowBufR[(size_t) i1] - wowBufR[(size_t) i0]);

            wowWritePos = (wowWritePos + 1) % size;
            wowPhase += wowInc;
            if (wowPhase > juce::MathConstants<float>::twoPi) wowPhase -= juce::MathConstants<float>::twoPi;
        }

        // 2. Bit crusher
        bcPhase += 1.f;
        if (bcPhase >= srDivInt)
        {
            bcPhase -= srDivInt;
            bcHeldL = std::round (l * bitLevels) / bitLevels;
            bcHeldR = std::round (r * bitLevels) / bitLevels;
        }
        l = bcHeldL;
        r = bcHeldR;

        // 3. Drive / saturation (soft-clip)
        if (drive > 0.001f)
        {
            float d = 1.f + drive * 9.f;
            l = std::tanh (l * d) / std::tanh (d);
            r = std::tanh (r * d) / std::tanh (d);
        }

        // 4. Vinyl noise + crackle -- only when signal is present
        float inputLevel = std::fabs (L[i]) + (R != nullptr ? std::fabs (R[i]) : 0.f);
        if (inputLevel > 0.0001f)
        {
            float noise = (rng.nextFloat() * 2.f - 1.f) * noiseAmt * 0.05f;
            float crackle = 0.f;
            if (crackleRate > 0.f && rng.nextFloat() < crackleRate * 0.001f)
                crackle = (rng.nextFloat() * 2.f - 1.f) * 0.4f;
            l += noise + crackle;
            r += noise + crackle;
        }

        // 5. Transient boost: gain rises only while the follower leads its
        // smoothed copy (an attack); steady material stays at unity gain
        float env = std::fabs (l + r) * 0.5f;
        envFast += (env > envFast ? fastAtt : envRel) * (env - envFast);
        envSlow += slowCoef * (envFast - envSlow);

        const float lead   = (envFast - envSlow) / (envFast + 1.0e-6f);
        const float attack = juce::jlimit (0.f, 1.f, (lead - 0.05f) / 0.95f);  // small deadband for ripple
        float transientGain = 1.f + transientBoost * 3.f * attack;
        l *= transientGain;
        r *= transientGain;

        L[i] = l;
        if (R != nullptr) R[i] = r;
    }

    // 6. Filters (HPF → LPF)
    {
        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        hpfFilter.process (ctx);
        lpfFilter.process (ctx);
    }

    // 7. Compressor
    {
        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        compressor.process (ctx);
        buffer.applyGain (compMakeupLin);
    }

    // 8. Reverb
    if (numChannels >= 2)
    {
        reverb.processStereo (buffer.getWritePointer(0),
                               buffer.getWritePointer(1),
                               numSamples);
    }
    else
    {
        reverb.processMono (buffer.getWritePointer(0), numSamples);
    }

    // 9. Stereo width (mid/side)
    if (R != nullptr && std::fabs (stereoWidth - 1.f) > 0.01f)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            float mid  = (L[i] + R[i]) * 0.5f;
            float side = (L[i] - R[i]) * 0.5f * stereoWidth;
            L[i] = mid + side;
            R[i] = mid - side;
        }
    }

    // 10. Output gain
    buffer.applyGain (outputGain);

    // Clip protection
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* d = buffer.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
            d[i] = juce::jlimit (-1.f, 1.f, d[i]);
    }
}

// ── State ─────────────────────────────────────────────────────────────────────
void DrumSmashProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    SharedProcessorUtils::saveState (*this, apvts, destData, currentPreset);
}

void DrumSmashProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    SharedProcessorUtils::loadState (*this, apvts, data, sizeInBytes, &currentPreset);
}

// ── Factory ───────────────────────────────────────────────────────────────────
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DrumSmashProcessor();
}