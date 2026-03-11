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
    for (int i = 0; i < kMaxHits; ++i)
        hitQueue[i].active = false;
}

BreakScientistProcessor::~BreakScientistProcessor() {}

// ── Parameter layout ──────────────────────────────────────────────────────────
juce::AudioProcessorValueTreeState::ParameterLayout
BreakScientistProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Swing: 50% = straight, 75% = very heavy shuffle
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (kSwing,       "Swing",        0.50f, 0.75f, 0.58f));

    // Humanize: random micro-offset depth, 0–1
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (kHumanize,    "Humanize",     0.00f, 1.00f, 0.20f));

    // Drag: consistent pull-back on all hits, 0–1 (maps to 0–80ms)
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (kDrag,        "Drag",         0.00f, 1.00f, 0.10f));

    // Sensitivity: transient detection, 0–1
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (kSensitivity, "Sensitivity",  0.00f, 1.00f, 0.50f));

    // Velocity Variance: reshapes hit amplitudes, 0–1
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (kVelocityVar, "Vel Variance", 0.00f, 1.00f, 0.30f));

    // Wet Mix
    params.push_back (std::make_unique<juce::AudioParameterFloat>
        (kWetMix,      "Wet Mix",      0.00f, 1.00f, 1.00f));

    return { params.begin(), params.end() };
}

// ── prepareToPlay ─────────────────────────────────────────────────────────────
void BreakScientistProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate;

    // Lookahead = 150ms max — enough headroom for drag + swing + humanize
    lookaheadSamples = (int)(kMaxLookaheadMs * 0.001 * sampleRate);
    setLatencySamples (lookaheadSamples);

    lookaheadL.assign (lookaheadSamples, 0.f);
    lookaheadR.assign (lookaheadSamples, 0.f);
    lookaheadWritePos = 0;

    hitBufL.assign (kHitBufSize, 0.f);
    hitBufR.assign (kHitBufSize, 0.f);
    hitBufWritePos  = 0;
    hitBufReadPos   = 0;
    hitBufRemaining = 0;

    crossfadeLen = (float)(0.005 * sampleRate);  // 5ms crossfade

    envelopeL      = 0.f;
    envelopeR      = 0.f;
    runningRms     = 0.f;
    rmsSmooth      = 0.f;
    inTransient    = false;
    transientCooldown = 0;

    gridPhase      = 0;
    sixteenthCount = 0;
    lastOnsetSample = 0;
    ioiEstimate    = 0.0;
    ioiCount       = 0;

    for (int i = 0; i < kMaxHits; ++i)
        hitQueue[i].active = false;
    hitQueueCount = 0;

    // Default BPM estimate — will be updated from host or IOI
    currentBpm       = 120.0;
    sixteenthSamples = (60.0 / currentBpm / 4.0) * sampleRate;

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
    if (numChannels < 1 || lookaheadSamples == 0) return;

    // Read params once per block
    const float swing       = apvts.getRawParameterValue (kSwing)->load();
    const float humanize    = apvts.getRawParameterValue (kHumanize)->load();
    const float drag        = apvts.getRawParameterValue (kDrag)->load();
    const float sensitivity = apvts.getRawParameterValue (kSensitivity)->load();
    const float velocityVar = apvts.getRawParameterValue (kVelocityVar)->load();
    const float wetMix      = apvts.getRawParameterValue (kWetMix)->load();

    // Update BPM from host playhead if available
    if (auto* ph = getPlayHead())
    {
        juce::AudioPlayHead::CurrentPositionInfo pos;
        if (ph->getCurrentPosition (pos) && pos.bpm > 20.0 && pos.bpm < 300.0)
        {
            currentBpm       = pos.bpm;
            sixteenthSamples = (60.0 / currentBpm / 4.0) * currentSampleRate;
        }
    }

    // Envelope follower coefficients (per-sample, applied in loop)
    // Attack fast (1ms), release slow (80ms)
    const float attackCoeff  = 1.f - std::exp (-1.f / (float)(0.001 * currentSampleRate));
    const float releaseCoeff = 1.f - std::exp (-1.f / (float)(0.080 * currentSampleRate));
    const float rmsCoeff     = 1.f - std::exp (-1.f / (float)(0.300 * currentSampleRate));

    // Detection threshold: higher sensitivity = lower threshold multiplier
    // sensitivity=0 → needs 6x RMS; sensitivity=1 → needs 1.5x RMS
    const float threshMult = 6.f - sensitivity * 4.5f;

    // Drag offset in samples (0–80ms)
    const int dragSamples = (int)(drag * 0.080f * (float)currentSampleRate);

    // Swing: even 16th notes play straight, odd 16th notes are delayed
    // swing=0.5 → 0 delay, swing=0.75 → delay = 0.5 * sixteenthSamples
    const int swingDelaySamples = (int)((swing - 0.5f) * 2.f * sixteenthSamples);

    // Max humanize offset: 0–30ms
    const int maxHumanizeSamples = (int)(humanize * 0.030f * (float)currentSampleRate);

    float* L = buffer.getWritePointer (0);
    float* R = (numChannels > 1) ? buffer.getWritePointer (1) : nullptr;

    // Dry copy for wet blend
    juce::AudioBuffer<float> dryBuffer;
    dryBuffer.makeCopyOf (buffer);
    const float* dryL = dryBuffer.getReadPointer (0);
    const float* dryR = (numChannels > 1) ? dryBuffer.getReadPointer (1) : nullptr;

    // We'll build the wet output into a scratch buffer, then blend at the end
    std::vector<float> wetL (numSamples, 0.f);
    std::vector<float> wetR (numSamples, 0.f);

    for (int i = 0; i < numSamples; ++i)
    {
        const float inL = L[i];
        const float inR = (R != nullptr) ? R[i] : inL;

        // ── 1. Write into lookahead circular buffer ───────────────────────────
        lookaheadL[lookaheadWritePos] = inL;
        lookaheadR[lookaheadWritePos] = inR;
        lookaheadWritePos = (lookaheadWritePos + 1) % lookaheadSamples;

        // ── 2. Envelope follower on incoming signal ───────────────────────────
        const float absL = std::fabs (inL);
        const float absR = std::fabs (inR);
        const float peak = std::max (absL, absR);

        if (peak > envelopeL)
            envelopeL += attackCoeff * (peak - envelopeL);
        else
            envelopeL += releaseCoeff * (0.f - envelopeL);

        // Running RMS (slow smoothed)
        rmsSmooth += rmsCoeff * (peak * peak - rmsSmooth);
        runningRms = std::sqrt (std::max (rmsSmooth, 1e-10f));

        // ── 3. Transient detection ────────────────────────────────────────────
        bool onsetDetected = false;
        if (transientCooldown > 0)
        {
            --transientCooldown;
        }
        else if (!inTransient && envelopeL > runningRms * threshMult && runningRms > 0.0001f)
        {
            onsetDetected = true;
            inTransient   = true;
            // Cooldown: don't re-trigger for ~40ms after each onset
            transientCooldown = (int)(0.040 * currentSampleRate);
        }
        else if (inTransient && envelopeL < runningRms * 0.5f)
        {
            inTransient = false;
        }

        // ── 4. Update 16th-note grid phase ───────────────────────────────────
        gridPhase++;
        if (sixteenthSamples > 1.0 && gridPhase >= (int)sixteenthSamples)
        {
            gridPhase = 0;
            sixteenthCount++;

            // Fallback IOI-based BPM estimation (when no host BPM)
            // (only used if we didn't get a valid host playhead above)
        }

        // ── 5. On onset: calculate displacement and queue the hit ─────────────
        if (onsetDetected && hitQueueCount < kMaxHits)
        {
            // Determine if this is an "odd" 16th note for swing
            const bool isOddSixteenth = (sixteenthCount % 2) == 1;
            const int  swingOffset    = isOddSixteenth ? swingDelaySamples : 0;

            // Random humanize offset — can be positive or negative
            int humanizeOffset = 0;
            if (maxHumanizeSamples > 0)
                humanizeOffset = rng.nextInt (maxHumanizeSamples * 2) - maxHumanizeSamples;

            // Drag always pulls back (positive = later)
            const int totalOffset = dragSamples + swingOffset + humanizeOffset;

            // Velocity variance: random gain scale around 1.0
            float gainScale = 1.f;
            if (velocityVar > 0.f)
            {
                // Range: (1 - velocityVar*0.5) to (1 + velocityVar*0.3)
                // Slightly asymmetric — accents louder than ghosts get quieter
                const float lo = 1.f - velocityVar * 0.5f;
                const float hi = 1.f + velocityVar * 0.3f;
                gainScale = lo + rng.nextFloat() * (hi - lo);
            }

            // Find a free hit slot
            for (int h = 0; h < kMaxHits; ++h)
            {
                if (!hitQueue[h].active)
                {
                    hitQueue[h].outputSampleOffset = totalOffset;
                    hitQueue[h].gainScale          = gainScale;
                    hitQueue[h].active             = true;
                    hitQueueCount++;
                    break;
                }
            }

            // Update IOI estimate for fallback BPM
            if (lastOnsetSample > 0 && ioiCount < 8)
            {
                const double ioi = (double)(i) - (double)lastOnsetSample;
                if (ioi > 1000 && ioi < 100000)
                {
                    ioiEstimate = (ioiEstimate * ioiCount + ioi) / (double)(ioiCount + 1);
                    ioiCount++;
                    // 4 onsets ≈ 1 beat → 16th = ioi/4... heuristic
                    if (ioiCount >= 4)
                        sixteenthSamples = ioiEstimate;
                }
            }
            lastOnsetSample = i;
        }

        // ── 6. For each queued hit, decrement its offset counter ──────────────
        //       When offset reaches 0, start reading from lookahead into hitBuf
        for (int h = 0; h < kMaxHits; ++h)
        {
            if (!hitQueue[h].active) continue;
            hitQueue[h].outputSampleOffset--;

            if (hitQueue[h].outputSampleOffset <= 0)
            {
                // Read the hit from lookahead at current read position
                // (which is lookaheadSamples behind write position = the onset)
                const int readPos = (lookaheadWritePos) % lookaheadSamples;
                const float hL = lookaheadL[readPos] * hitQueue[h].gainScale;
                const float hR = lookaheadR[readPos] * hitQueue[h].gainScale;

                // Write into hit output buffer with crossfade envelope
                const int wPos = hitBufWritePos % kHitBufSize;
                hitBufL[wPos] = hL;
                hitBufR[wPos] = hR;
                hitBufWritePos = (hitBufWritePos + 1) % kHitBufSize;
                hitBufRemaining = std::max (hitBufRemaining,
                    (int)(crossfadeLen * 2.f + (int)(0.080 * currentSampleRate)));

                hitQueue[h].active = false;
                hitQueueCount = std::max (0, hitQueueCount - 1);
            }
        }

        // ── 7. Build wet output ───────────────────────────────────────────────
        // The wet signal is the lookahead-delayed, displaced audio.
        // Simple approach: pass lookahead audio through with displacement applied.
        // The lookahead read position is `lookaheadSamples` behind write.
        const int delayedReadPos = (lookaheadWritePos + 0) % lookaheadSamples;
        wetL[i] = lookaheadL[delayedReadPos] * (hitBufRemaining > 0 ? 0.f : 1.f);
        wetR[i] = lookaheadR[delayedReadPos] * (hitBufRemaining > 0 ? 0.f : 1.f);

        // Output hit buffer content (the displaced hit)
        if (hitBufRemaining > 0)
        {
            const int rPos = hitBufReadPos % kHitBufSize;

            // Crossfade envelope
            float env = 1.f;
            const int fadeLen = (int)crossfadeLen;
            const int pos = (int)(hitBufReadPos % kHitBufSize);
            if (pos < fadeLen)
                env = (float)pos / (float)fadeLen;
            else if (hitBufRemaining < fadeLen)
                env = (float)hitBufRemaining / (float)fadeLen;

            wetL[i] += hitBufL[rPos] * env;
            wetR[i] += hitBufR[rPos] * env;
            hitBufReadPos = (hitBufReadPos + 1) % kHitBufSize;
            hitBufRemaining--;
        }
    }

    // ── 8. Blend dry and wet ──────────────────────────────────────────────────
    for (int i = 0; i < numSamples; ++i)
    {
        L[i] = dryL[i] * (1.f - wetMix) + wetL[i] * wetMix;
        if (R != nullptr && dryR != nullptr)
            R[i] = dryR[i] * (1.f - wetMix) + wetR[i] * wetMix;
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
    return new juce::GenericAudioProcessorEditor (*this);
}

// ── Plugin entry point ────────────────────────────────────────────────────────
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BreakScientistProcessor();
}