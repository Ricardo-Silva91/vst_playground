#include "PluginProcessor.h"
#include "PluginEditor.h"

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
}

DrumSmashProcessor::~DrumSmashProcessor() {}

// ── Parameter layout ──────────────────────────────────────────────────────────
juce::AudioProcessorValueTreeState::ParameterLayout
DrumSmashProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (juce::ParameterID{kBitDepth, 1},      "Bit Depth",       1.f,  16.f, 16.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (juce::ParameterID{kSampleRateDiv, 1}, "SR Divide",        1.f,  32.f,  1.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (juce::ParameterID{kDrive, 1},         "Drive",            0.f,   1.f,  0.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (juce::ParameterID{kOutputGain, 1},    "Output Gain",      0.f,   2.f,  1.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (juce::ParameterID{kNoiseAmount, 1},   "Noise Amount",     0.f,   1.f,  0.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (juce::ParameterID{kCrackleRate, 1},   "Crackle Rate",     0.f,   1.f,  0.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (juce::ParameterID{kLpfCutoff, 1},     "LPF Cutoff",     200.f, 22000.f, 22000.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (juce::ParameterID{kHpfCutoff, 1},     "HPF Cutoff",      20.f,  2000.f,  20.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (juce::ParameterID{kCompThreshold, 1}, "Comp Threshold", -60.f,    0.f, -18.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (juce::ParameterID{kCompRatio, 1},     "Comp Ratio",       1.f,   20.f,  3.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (juce::ParameterID{kCompAttack, 1},    "Comp Attack ms",   0.1f, 200.f, 10.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (juce::ParameterID{kCompRelease, 1},   "Comp Release ms", 10.f, 2000.f, 150.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (juce::ParameterID{kCompMakeup, 1},    "Comp Makeup dB",   0.f,  24.f,  0.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (juce::ParameterID{kReverbRoom, 1},    "Reverb Room",      0.f,   1.f,  0.3f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (juce::ParameterID{kReverbWet, 1},     "Reverb Wet",       0.f,   1.f,  0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (juce::ParameterID{kReverbDamping, 1}, "Reverb Damping",   0.f,   1.f,  0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (juce::ParameterID{kPitchSemitones, 1},"Pitch Semitones", -12.f,  12.f,  0.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (juce::ParameterID{kWowRate, 1},       "Wow/Flutter Hz",   0.f,   8.f,  0.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (juce::ParameterID{kWowDepth, 1},      "Wow/Flutter Cents",0.f,  50.f,  0.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (juce::ParameterID{kStereoWidth, 1},   "Stereo Width",     0.f,   2.f,  1.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (juce::ParameterID{kTransientBoost, 1},"Transient Boost",  0.f,   1.f,  0.f));

    return { params.begin(), params.end() };
}

// ── Preset loading ────────────────────────────────────────────────────────────
void DrumSmashProcessor::applyPreset (int index)
{
    if (index < 0 || index >= kNumPresets) return;
    currentPreset = index;
    const auto& p = kPresets[index];

    apvts.getParameter (kBitDepth)      ->setValueNotifyingHost (apvts.getParameter(kBitDepth)->convertTo0to1 (p.bitDepth));
    apvts.getParameter (kSampleRateDiv) ->setValueNotifyingHost (apvts.getParameter(kSampleRateDiv)->convertTo0to1 (p.sampleRateDiv));
    apvts.getParameter (kDrive)         ->setValueNotifyingHost (apvts.getParameter(kDrive)->convertTo0to1 (p.drive));
    apvts.getParameter (kOutputGain)    ->setValueNotifyingHost (apvts.getParameter(kOutputGain)->convertTo0to1 (p.outputGain));
    apvts.getParameter (kNoiseAmount)   ->setValueNotifyingHost (apvts.getParameter(kNoiseAmount)->convertTo0to1 (p.noiseAmount));
    apvts.getParameter (kCrackleRate)   ->setValueNotifyingHost (apvts.getParameter(kCrackleRate)->convertTo0to1 (p.crackleRate));
    apvts.getParameter (kLpfCutoff)     ->setValueNotifyingHost (apvts.getParameter(kLpfCutoff)->convertTo0to1 (p.lpfCutoff));
    apvts.getParameter (kHpfCutoff)     ->setValueNotifyingHost (apvts.getParameter(kHpfCutoff)->convertTo0to1 (p.hpfCutoff));
    apvts.getParameter (kCompThreshold) ->setValueNotifyingHost (apvts.getParameter(kCompThreshold)->convertTo0to1 (p.compThresholdDb));
    apvts.getParameter (kCompRatio)     ->setValueNotifyingHost (apvts.getParameter(kCompRatio)->convertTo0to1 (p.compRatio));
    apvts.getParameter (kCompAttack)    ->setValueNotifyingHost (apvts.getParameter(kCompAttack)->convertTo0to1 (p.compAttackMs));
    apvts.getParameter (kCompRelease)   ->setValueNotifyingHost (apvts.getParameter(kCompRelease)->convertTo0to1 (p.compReleaseMs));
    apvts.getParameter (kCompMakeup)    ->setValueNotifyingHost (apvts.getParameter(kCompMakeup)->convertTo0to1 (p.compMakeupDb));
    apvts.getParameter (kReverbRoom)    ->setValueNotifyingHost (apvts.getParameter(kReverbRoom)->convertTo0to1 (p.reverbRoomSize));
    apvts.getParameter (kReverbWet)     ->setValueNotifyingHost (apvts.getParameter(kReverbWet)->convertTo0to1 (p.reverbWet));
    apvts.getParameter (kReverbDamping) ->setValueNotifyingHost (apvts.getParameter(kReverbDamping)->convertTo0to1 (p.reverbDamping));
    apvts.getParameter (kPitchSemitones)->setValueNotifyingHost (apvts.getParameter(kPitchSemitones)->convertTo0to1 (p.pitchSemitones));
    apvts.getParameter (kWowRate)       ->setValueNotifyingHost (apvts.getParameter(kWowRate)->convertTo0to1 (p.wowFlutterRate));
    apvts.getParameter (kWowDepth)      ->setValueNotifyingHost (apvts.getParameter(kWowDepth)->convertTo0to1 (p.wowFlutterDepth));
    apvts.getParameter (kStereoWidth)   ->setValueNotifyingHost (apvts.getParameter(kStereoWidth)->convertTo0to1 (p.stereoWidth));
    apvts.getParameter (kTransientBoost)->setValueNotifyingHost (apvts.getParameter(kTransientBoost)->convertTo0to1 (p.transientBoost));

    rebuildDSP();
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

    auto& hpfNode = filterChain.get<0>();
    auto& lpfNode = filterChain.get<1>();

    *hpfNode.coefficients = *juce::dsp::IIR::Coefficients<float>::makeHighPass (currentSampleRate, hpf, 0.707f);
    *lpfNode.coefficients = *juce::dsp::IIR::Coefficients<float>::makeLowPass  (currentSampleRate, lpf, 0.707f);

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
    envFollower = 0.f;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels      = 2;

    filterChain.prepare (spec);
    compressor.prepare  (spec);
    reverb.reset();

    rebuildDSP();
    applyPreset (currentPreset);
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

    // Rebuild filter/comp/reverb coefficients every block (cheap enough for
    // low-freq params; a proper impl would use smoothed values)
    rebuildDSP();

    float* L = buffer.getWritePointer (0);
    float* R = (numChannels > 1) ? buffer.getWritePointer (1) : nullptr;

    // ── Per-sample processing ────────────────────────────────────────────────
    for (int i = 0; i < numSamples; ++i)
    {
        float l = L[i];
        float r = (R != nullptr) ? R[i] : l;

        // 1. Pitch shift via simple resampling (naive — sounds lo-fi by design)
        if (std::fabs (pitchRatio - 1.f) > 0.001f || wowDepth > 0.f)
        {
            float wow = (wowDepth > 0.f && wowRate > 0.f)
                ? (wowDepth / 1200.f) * std::sin (wowPhase)
                : 0.f;
            wowPhase += juce::MathConstants<float>::twoPi * wowRate / (float)currentSampleRate;
            if (wowPhase > juce::MathConstants<float>::twoPi) wowPhase -= juce::MathConstants<float>::twoPi;

            // Simple pitch: scale amplitude envelope, no true re-pitch buffer
            float wowGain = std::pow (2.f, wow);
            l *= wowGain;
            r *= wowGain;
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

        // 4. Vinyl noise + crackle
        float noise = (rng.nextFloat() * 2.f - 1.f) * noiseAmt * 0.05f;
        float crackle = 0.f;
        if (crackleRate > 0.f && rng.nextFloat() < crackleRate * 0.001f)
            crackle = (rng.nextFloat() * 2.f - 1.f) * 0.4f;
        l += noise + crackle;
        r += noise + crackle;

        // 5. Transient boost (envelope follower → gain boost on attacks)
        float env = std::fabs (l + r) * 0.5f;
        float attackCoef  = 1.f - std::exp (-1.f / (0.001f * (float)currentSampleRate));
        float releaseCoef = 1.f - std::exp (-1.f / (0.050f * (float)currentSampleRate));
        if (env > envFollower)
            envFollower += attackCoef  * (env - envFollower);
        else
            envFollower += releaseCoef * (env - envFollower);

        float transientGain = 1.f + transientBoost * 3.f * juce::jlimit(0.f, 1.f, env - envFollower + 0.5f);
        l *= transientGain;
        r *= transientGain;

        L[i] = l;
        if (R != nullptr) R[i] = r;
    }

    // 6. Filters (HPF → LPF)
    {
        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        filterChain.process (ctx);
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
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    xml->setAttribute ("currentPreset", currentPreset);
    copyXmlToBinary (*xml, destData);
}

void DrumSmashProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
    {
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
        currentPreset = xml->getIntAttribute ("currentPreset", 0);
    }
}

// ── Factory ───────────────────────────────────────────────────────────────────
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DrumSmashProcessor();
}
