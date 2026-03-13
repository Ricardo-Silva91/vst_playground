#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <algorithm>

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

    // Swing: 0.50=straight, 0.85=extreme broken
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (kSwing,       "Swing",        0.50f, 0.85f, 0.58f));
    // Humanize: ±80ms random offset
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (kHumanize,    "Humanize",     0.00f, 1.00f, 0.20f));
    // Drag: 0–200ms consistent pull-back
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (kDrag,        "Drag",         0.00f, 1.00f, 0.10f));
    // Sensitivity: 0=only biggest hits, 1=catch everything
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

    lookaheadSamples = (int)(kLookaheadMs * 0.001 * sampleRate);
    setLatencySamples (lookaheadSamples);

    // Ring size: lookahead + max displacement + hit window + safety margin
    const int extraSamples = (int)((kMaxDisplaceMs * 0.001 + kHitWindowSec + 0.1) * sampleRate);
    ringSize = lookaheadSamples + extraSamples;

    ringInL.assign  (ringSize, 0.f);
    ringInR.assign  (ringSize, 0.f);
    ringOutL.assign (ringSize, 0.f);
    ringOutR.assign (ringSize, 0.f);
    ringMask.assign (ringSize, 0.f);

    ringWritePos = 0;
    ringReadPos  = ringSize - lookaheadSamples;

    envelope        = 0.f;
    rmsSmooth       = 0.f;
    runningRms      = 0.f;
    inTransient     = false;
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

    const float swing       = apvts.getRawParameterValue (kSwing)->load();
    const float humanize    = apvts.getRawParameterValue (kHumanize)->load();
    const float drag        = apvts.getRawParameterValue (kDrag)->load();
    const float sensitivity = apvts.getRawParameterValue (kSensitivity)->load();
    const float velocityVar = apvts.getRawParameterValue (kVelocityVar)->load();
    const float wetMix      = apvts.getRawParameterValue (kWetMix)->load();

    // Host BPM
    if (auto* ph = getPlayHead())
    {
        juce::AudioPlayHead::CurrentPositionInfo pos;
        if (ph->getCurrentPosition (pos) && pos.bpm > 20.0 && pos.bpm < 300.0)
        {
            currentBpm       = pos.bpm;
            sixteenthSamples = (60.0 / currentBpm / 4.0) * currentSampleRate;
        }
    }

    // Envelope follower — fast attack (0.5ms), medium release (60ms)
    const float attackCoeff  = 1.f - std::exp (-1.f / (float)(0.0005 * currentSampleRate));
    const float releaseCoeff = 1.f - std::exp (-1.f / (float)(0.060  * currentSampleRate));
    // Slow RMS for threshold reference (500ms)
    const float rmsCoeff     = 1.f - std::exp (-1.f / (float)(0.500  * currentSampleRate));

    // Sensitivity: maps 0→1 to a threshold multiplier of 4.0→1.2
    // Lower multiplier = catches more hits. Range tightened vs previous
    // version so sensitivity=0.5 already catches most kick/snare hits.
    const float threshMult = 4.0f - sensitivity * 2.8f;

    // Displacement params
    const int dragSamples        = (int)(drag     * 0.200f * (float)currentSampleRate);
    const int swingDelaySamples  = (int)((swing - 0.5f) * 2.f * (float)sixteenthSamples);
    const int maxHumanizeSamples = (int)(humanize * 0.080f * (float)currentSampleRate);

    const int hitWindowSamples  = (int)(kHitWindowSec    * currentSampleRate);
    const int peakScanSamples   = (int)(kPeakScanSec     * currentSampleRate);
    const float* inL  = buffer.getReadPointer (0);
    const float* inR  = (numChannels > 1) ? buffer.getReadPointer (1) : inL;
    float*       outL = buffer.getWritePointer (0);
    float*       outR = (numChannels > 1) ? buffer.getWritePointer (1) : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        const float sL = inL[i];
        const float sR = inR[i];

        // ── 1. Write into input ring ──────────────────────────────────────────
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

        // ── 3. Onset detection ────────────────────────────────────────────────
        if (cooldownSamples > 0)
        {
            --cooldownSamples;
        }
        else if (!inTransient && runningRms > 0.00005f && envelope > runningRms * threshMult)
        {
            inTransient     = true;
            cooldownSamples = (int)(0.050 * currentSampleRate);

            // ── 4. Find true peak within next peakScanSamples ─────────────────
            // We have the full future in our ring buffer — scan forward to find
            // where the hit actually peaks. This anchors the Hann window at the
            // real attack transient, not just where energy started rising.
            int   peakOffset = 0;
            float peakVal    = 0.f;
            for (int s = 0; s < peakScanSamples; ++s)
            {
                const int scanPos = (ringWritePos + s) % ringSize;
                const float v = std::max (std::fabs (ringInL[scanPos]),
                                          std::fabs (ringInR[scanPos]));
                if (v > peakVal) { peakVal = v; peakOffset = s; }
            }

            // Onset anchor = ringWritePos + peakOffset
            const int onsetPos = (ringWritePos + peakOffset) % ringSize;

            // ── 5. Calculate displacement ─────────────────────────────────────
            const bool isOddSixteenth = (sixteenthCount % 2) == 1;
            const int  swingOffset    = isOddSixteenth ? swingDelaySamples : 0;

            int humanizeOffset = 0;
            if (maxHumanizeSamples > 0)
                humanizeOffset = rng.nextInt (maxHumanizeSamples * 2) - maxHumanizeSamples;

            const int rawOffset   = dragSamples + swingOffset + humanizeOffset;
            const int totalOffset = juce::jlimit (-(lookaheadSamples - hitWindowSamples),
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

            // ── 6. Suppress original hit in output ring ───────────────────────
            // Scan forward from the peak to find where the hit envelope drops
            // to 15% of the peak value — that's the natural end of the hit.
            // Only suppress up to that point, capped hard at 80ms.
            // This prevents blanking out the space between hits on sparse breaks.
            const int   maxSuppressSamples = (int)(0.080 * currentSampleRate);
            const float suppressThresh     = peakVal * 0.15f;
            int suppressEnd = maxSuppressSamples;  // default: full cap
            for (int s = peakScanSamples; s < maxSuppressSamples; ++s)
            {
                const int   scanPos = (onsetPos + s) % ringSize;
                const float v = std::max (std::fabs (ringInL[scanPos]),
                                          std::fabs (ringInR[scanPos]));
                if (v < suppressThresh) { suppressEnd = s; break; }
            }

            for (int s = 0; s < suppressEnd; ++s)
            {
                const int mPos = (onsetPos + s) % ringSize;
                ringMask[mPos] = 1.f;
            }

            // ── 7. Copy hit into output ring at displaced position ─────────────
            // destStart is where the displaced hit should appear in output-time.
            // The formula maps onsetPos (write-side) to its corresponding
            // read-side position, then shifts by totalOffset.
            //
            //   write-side pos → read-side pos:
            //   readSideOnset = onsetPos - lookaheadSamples  (mod ringSize)
            //
            // We then add totalOffset to displace it.
            const int readSideOnset = (onsetPos - lookaheadSamples + ringSize) % ringSize;
            const int destStart     = (readSideOnset + totalOffset + ringSize) % ringSize;

            // Hann window applied only to the OUTPUT copy, centred on the hit.
            // The window is slightly asymmetric: short fade-in (10% of window)
            // before the peak, longer fade-out (90%) after — preserves attack punch.
            const int fadeInSamples  = hitWindowSamples / 10;
            const int fadeOutSamples = hitWindowSamples - fadeInSamples;

            for (int w = 0; w < hitWindowSamples; ++w)
            {
                float env;
                if (w < fadeInSamples)
                {
                    // Short fade in before the attack
                    env = 0.5f * (1.f - std::cos (juce::MathConstants<float>::pi
                                                   * (float)w / (float)fadeInSamples));
                }
                else
                {
                    // Longer cosine fade out after the peak
                    const int wo = w - fadeInSamples;
                    env = 0.5f * (1.f + std::cos (juce::MathConstants<float>::pi
                                                   * (float)wo / (float)fadeOutSamples));
                }

                const int srcPos = (onsetPos + w) % ringSize;
                const int dstPos = (destStart + w) % ringSize;

                ringOutL[dstPos] += ringInL[srcPos] * env * gainScale;
                ringOutR[dstPos] += ringInR[srcPos] * env * gainScale;
            }

            // ── 8. IOI fallback BPM ───────────────────────────────────────────
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
        else if (inTransient && envelope < runningRms * 0.35f)
        {
            inTransient = false;
        }

        // ── 9. Advance grid phase ─────────────────────────────────────────────
        if (sixteenthSamples > 1.0)
        {
            if (++gridPhase >= (int)sixteenthSamples)
            {
                gridPhase = 0;
                ++sixteenthCount;
            }
        }

        // ── 10. Read from output ring ─────────────────────────────────────────
        const float mask  = ringMask[ringReadPos];
        const float pL    = ringInL[ringReadPos];
        const float pR    = ringInR[ringReadPos];
        const float hL    = ringOutL[ringReadPos];
        const float hR    = ringOutR[ringReadPos];

        // Where mask=1: displaced hit only, original fully suppressed.
        // Where mask=0: original pass-through (undetected hits, hi-hats, etc.)
        const float wetL = hL + pL * (1.f - mask);
        const float wetR = hR + pR * (1.f - mask);

        // Clear ring position after reading
        ringOutL[ringReadPos] = 0.f;
        ringOutR[ringReadPos] = 0.f;
        ringMask[ringReadPos] = 0.f;

        outL[i] = sL * (1.f - wetMix) + wetL * wetMix;
        if (outR != nullptr)
            outR[i] = sR * (1.f - wetMix) + wetR * wetMix;

        // ── 11. Advance ring positions ────────────────────────────────────────
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

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BreakScientistProcessor();
}