#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <algorithm>

// ── Parameter IDs ─────────────────────────────────────────────────────────────
static const juce::String kSwing       = "swing";
static const juce::String kHumanize    = "humanize";
static const juce::String kDrag        = "drag";
static const juce::String kSensitivity = "sensitivity";
static const juce::String kVelocityVar = "velocityvar";
static const juce::String kWetMix      = "wetmix";

// ── Constructor ───────────────────────────────────────────────────────────────
BreakScientistProcessor::BreakScientistProcessor()
    : AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
}

BreakScientistProcessor::~BreakScientistProcessor() {}

// ── Parameter layout ──────────────────────────────────────────────────────────
juce::AudioProcessorValueTreeState::ParameterLayout
BreakScientistProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (kSwing,       "Swing",        0.50f, 0.85f, 0.58f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (kHumanize,    "Humanize",     0.00f, 1.00f, 0.20f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (kDrag,        "Drag",         0.00f, 1.00f, 0.10f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (kSensitivity, "Sensitivity",  0.00f, 1.00f, 0.50f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (kVelocityVar, "Vel Variance", 0.00f, 1.00f, 0.30f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (kWetMix,      "Wet Mix",      0.00f, 1.00f, 1.00f));

    return { params.begin(), params.end() };
}

// ── prepareToPlay ─────────────────────────────────────────────────────────────
void BreakScientistProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate;

    // Lookahead = max possible displacement so we can always advance OR delay hits
    lookaheadSamples = (int)(kMaxDisplaceMs * 0.001 * sampleRate);
    setLatencySamples (lookaheadSamples);

    // Ring buffer = 2x lookahead — plenty of runway on both sides
    ringSize = lookaheadSamples * 4;

    ringInL.assign  (ringSize, 0.f);
    ringInR.assign  (ringSize, 0.f);
    ringOutL.assign (ringSize, 0.f);
    ringOutR.assign (ringSize, 0.f);
    ringOutMask.assign (ringSize, 0.f);

    ringWritePos = 0;
    ringReadPos  = ringSize - lookaheadSamples;  // starts lookahead behind write

    envelope      = 0.f;
    rmsSmooth     = 0.f;
    runningRms    = 0.f;
    inTransient   = false;
    cooldownSamples = 0;

    currentBpm       = 120.0;
    sixteenthSamples = (60.0 / currentBpm / 4.0) * sampleRate;
    sixteenthCount   = 0;
    gridPhase        = 0;

    lastOnsetRingPos = -1;
    ioiEstimate      = 0.0;
    ioiCount         = 0;

    applyPreset (currentPreset);
}

void BreakScientistProcessor::releaseResources() {}

// ── processBlock ──────────────────────────────────────────────────────────────
void BreakScientistProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                             juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    if (numChannels < 1 || ringSize == 0) return;

    // ── Read params ───────────────────────────────────────────────────────────
    const float swing       = apvts.getRawParameterValue (kSwing)->load();
    const float humanize    = apvts.getRawParameterValue (kHumanize)->load();
    const float drag        = apvts.getRawParameterValue (kDrag)->load();
    const float sensitivity = apvts.getRawParameterValue (kSensitivity)->load();
    const float velocityVar = apvts.getRawParameterValue (kVelocityVar)->load();
    const float wetMix      = apvts.getRawParameterValue (kWetMix)->load();

    // ── Host BPM ──────────────────────────────────────────────────────────────
    if (auto* ph = getPlayHead())
    {
        juce::AudioPlayHead::CurrentPositionInfo pos;
        if (ph->getCurrentPosition (pos) && pos.bpm > 20.0 && pos.bpm < 300.0)
        {
            currentBpm       = pos.bpm;
            sixteenthSamples = (60.0 / currentBpm / 4.0) * currentSampleRate;
        }
    }

    // ── Envelope follower coefficients ────────────────────────────────────────
    const float attackCoeff  = 1.f - std::exp (-1.f / (float)(0.001 * currentSampleRate));
    const float releaseCoeff = 1.f - std::exp (-1.f / (float)(0.080 * currentSampleRate));
    const float rmsCoeff     = 1.f - std::exp (-1.f / (float)(0.400 * currentSampleRate));

    // Onset threshold: sensitivity=0 → 6x RMS, sensitivity=1 → 1.5x RMS
    const float threshMult = 6.f - sensitivity * 4.5f;

    // Displacement amounts in samples
    const int dragSamples         = (int)(drag * 0.200f * (float)currentSampleRate);
    const int swingDelaySamples   = (int)((swing - 0.5f) * 2.f * sixteenthSamples);
    const int maxHumanizeSamples  = (int)(humanize * 0.080f * (float)currentSampleRate);

    // Hit window length — how many samples we copy per detected hit (~120ms)
    const int hitWindowSamples = (int)(0.120 * currentSampleRate);

    // Crossfade length for Hann window — applied to every copied hit
    // The full hit window uses a Hann envelope so it always fades in and out
    // cleanly regardless of what the audio is doing at the edges.

    const float* inL = buffer.getReadPointer (0);
    const float* inR = (numChannels > 1) ? buffer.getReadPointer (1) : inL;
    float*       outL = buffer.getWritePointer (0);
    float*       outR = (numChannels > 1) ? buffer.getWritePointer (1) : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        const float sL = inL[i];
        const float sR = inR[i];

        // ── 1. Write incoming sample into input ring ──────────────────────────
        ringInL[ringWritePos] = sL;
        ringInR[ringWritePos] = sR;

        // ── 2. Envelope follower ──────────────────────────────────────────────
        const float peak = std::max (std::fabs (sL), std::fabs (sR));

        if (peak > envelope)
            envelope += attackCoeff * (peak - envelope);
        else
            envelope += releaseCoeff * (0.f - envelope);

        rmsSmooth  += rmsCoeff * (peak * peak - rmsSmooth);
        runningRms  = std::sqrt (std::max (rmsSmooth, 1e-10f));

        // ── 3. Transient detection ────────────────────────────────────────────
        if (cooldownSamples > 0)
        {
            --cooldownSamples;
        }
        else if (!inTransient && runningRms > 0.0001f && envelope > runningRms * threshMult)
        {
            inTransient     = true;
            cooldownSamples = (int)(0.050 * currentSampleRate);  // 50ms cooldown

            // ── 4. Calculate displacement ─────────────────────────────────────
            const bool isOddSixteenth = (sixteenthCount % 2) == 1;
            const int  swingOffset    = isOddSixteenth ? swingDelaySamples : 0;

            int humanizeOffset = 0;
            if (maxHumanizeSamples > 0)
                humanizeOffset = rng.nextInt (maxHumanizeSamples * 2) - maxHumanizeSamples;

            // Total displacement: positive = later, negative = earlier
            // Clamp to stay within lookahead budget
            const int rawOffset   = dragSamples + swingOffset + humanizeOffset;
            const int totalOffset = juce::jlimit (-lookaheadSamples + hitWindowSamples,
                                                   lookaheadSamples - hitWindowSamples,
                                                   rawOffset);

            // Velocity variance
            float gainScale = 1.f;
            if (velocityVar > 0.f)
            {
                const float lo = 1.f - velocityVar * 0.5f;
                const float hi = 1.f + velocityVar * 0.3f;
                gainScale = lo + rng.nextFloat() * (hi - lo);
            }

            // ── 5. Copy hit window into output ring at displaced position ─────
            //
            // Source: input ring starting at ringWritePos (the onset)
            // Destination: output ring at (ringWritePos + lookaheadSamples + totalOffset)
            //   → the lookaheadSamples term keeps us aligned with the read pointer
            //   → totalOffset shifts earlier or later within that window
            //
            // We apply a Hann window envelope to the entire copied window so
            // edges are always zero — no clicks regardless of audio content.

            const int destStart = (ringWritePos + lookaheadSamples + totalOffset + ringSize) % ringSize;

            for (int w = 0; w < hitWindowSamples; ++w)
            {
                // Hann envelope: 0 at edges, 1 at centre
                const float hann = 0.5f * (1.f - std::cos (2.f * juce::MathConstants<float>::pi
                                                             * (float)w / (float)(hitWindowSamples - 1)));

                const int srcPos  = (ringWritePos + w) % ringSize;
                const int dstPos  = (destStart + w)    % ringSize;

                const float scaledHann = hann * gainScale;

                // Accumulate — multiple hits can overlap cleanly
                ringOutL[dstPos] += ringInL[srcPos] * scaledHann;
                ringOutR[dstPos] += ringInR[srcPos] * scaledHann;

                // Mark this output position as "hit present" so pass-through
                // is suppressed. Use max so overlapping hits don't fight.
                ringOutMask[dstPos] = std::min (ringOutMask[dstPos] + hann, 1.f);
            }

            // ── 6. IOI-based BPM fallback ─────────────────────────────────────
            if (lastOnsetRingPos >= 0 && ioiCount < 16)
            {
                int ioi = ringWritePos - lastOnsetRingPos;
                if (ioi < 0) ioi += ringSize;
                if (ioi > (int)(0.05 * currentSampleRate) &&
                    ioi < (int)(2.0  * currentSampleRate))
                {
                    ioiEstimate = (ioiEstimate * ioiCount + ioi) / (double)(ioiCount + 1);
                    ++ioiCount;
                    if (ioiCount >= 4)
                        sixteenthSamples = ioiEstimate;
                }
            }
            lastOnsetRingPos = ringWritePos;
        }
        else if (inTransient && envelope < runningRms * 0.4f)
        {
            inTransient = false;
        }

        // ── 7. Advance grid phase ─────────────────────────────────────────────
        if (sixteenthSamples > 1.0)
        {
            ++gridPhase;
            if (gridPhase >= (int)sixteenthSamples)
            {
                gridPhase = 0;
                ++sixteenthCount;
            }
        }

        // ── 8. Read from output ring — mix displaced hits + pass-through ──────
        //
        // At the read position:
        //   - If ringOutMask > 0: a displaced hit was written here.
        //     Output the hit and suppress the pass-through proportionally.
        //   - Otherwise: pure pass-through from input ring (delayed by lookahead).
        //
        // This means untouched beats play at their original times, while
        // detected hits play at their displaced positions.

        const float mask      = ringOutMask[ringReadPos];
        const float passL     = ringInL[ringReadPos];
        const float passR     = ringInR[ringReadPos];
        const float hitL      = ringOutL[ringReadPos];
        const float hitR      = ringOutR[ringReadPos];

        const float wetL_samp = hitL + passL * (1.f - mask);
        const float wetR_samp = hitR + passR * (1.f - mask);

        // Clear output ring position after reading so it's clean next time around
        ringOutL[ringReadPos]    = 0.f;
        ringOutR[ringReadPos]    = 0.f;
        ringOutMask[ringReadPos] = 0.f;

        // Wet/dry blend
        outL[i] = sL * (1.f - wetMix) + wetL_samp * wetMix;
        if (outR != nullptr)
            outR[i] = sR * (1.f - wetMix) + wetR_samp * wetMix;

        // ── 9. Advance ring positions ─────────────────────────────────────────
        ringWritePos = (ringWritePos + 1) % ringSize;
        ringReadPos  = (ringReadPos  + 1) % ringSize;
    }
}

// ── Preset system ─────────────────────────────────────────────────────────────
void BreakScientistProcessor::applyPreset (int index)
{
    if (index < 0 || index >= kNumPresets) return;
    currentPreset = index;
    const auto& p = kPresets[index];

    apvts.getParameter (kSwing)->setValueNotifyingHost (
        apvts.getParameter (kSwing)->convertTo0to1 (p.swing));
    apvts.getParameter (kHumanize)->setValueNotifyingHost (
        apvts.getParameter (kHumanize)->convertTo0to1 (p.humanize));
    apvts.getParameter (kDrag)->setValueNotifyingHost (
        apvts.getParameter (kDrag)->convertTo0to1 (p.drag));
    apvts.getParameter (kSensitivity)->setValueNotifyingHost (
        apvts.getParameter (kSensitivity)->convertTo0to1 (p.sensitivity));
    apvts.getParameter (kVelocityVar)->setValueNotifyingHost (
        apvts.getParameter (kVelocityVar)->convertTo0to1 (p.velocityVar));
    apvts.getParameter (kWetMix)->setValueNotifyingHost (
        apvts.getParameter (kWetMix)->convertTo0to1 (p.wetMix));
}

void BreakScientistProcessor::setCurrentProgram (int index) { applyPreset (index); }

const juce::String BreakScientistProcessor::getProgramName (int index)
{
    if (index >= 0 && index < kNumPresets) return kPresets[index].name;
    return "Unknown";
}

// ── State save/load ───────────────────────────────────────────────────────────
void BreakScientistProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    xml->setAttribute ("currentPreset", currentPreset);
    copyXmlToBinary (*xml, destData);
}

void BreakScientistProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
    {
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
        currentPreset = xml->getIntAttribute ("currentPreset", 0);
    }
}

// ── Editor factory ────────────────────────────────────────────────────────────
juce::AudioProcessorEditor* BreakScientistProcessor::createEditor()
{
    return new BreakScientistEditor (*this);
}

// ── Plugin entry point ────────────────────────────────────────────────────────
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BreakScientistProcessor();
}