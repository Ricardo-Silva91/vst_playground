// =============================================================================
//  LizardTape — core lo-fi tape DSP  (Source/DSP/TapeProcessor.h)
//  Part of the "Lizard" JUCE effects family.
//
//  JUCE-INDEPENDENT (only <cmath>, <vector>, <random>, <algorithm>), so the
//  same code that runs in the plugin compiles and unit-tests with plain g++.
//  See test/tape_test.cpp.
//
//  Signal chain (mono core; one instance per channel):
//     input -> drive+asymmetric tanh saturation -> wow&flutter (cubic frac
//     delay) -> tone (one-pole HF roll-off) -> hiss (program-dependent) ->
//     dry/wet mix + output trim
// =============================================================================
#pragma once

#include <cmath>
#include <vector>
#include <random>
#include <algorithm>

namespace lizard
{

class SmoothedValue
{
public:
    void reset (double sampleRate, double rampSeconds = 0.02)
    {
        const double t = std::max (rampSeconds, 1.0e-5);
        coeff = std::exp (-1.0 / (t * sampleRate));
        current = target;
    }
    void setTarget (float v) noexcept { target = v; }
    void snap      (float v) noexcept { target = current = v; }
    float next () noexcept
    {
        current = target + (current - target) * coeff;
        return static_cast<float> (current);
    }
    float peek () const noexcept { return static_cast<float> (current); }
private:
    double coeff = 0.0, target = 0.0, current = 0.0;
};

// Modulated fractional delay with Catmull-Rom (cubic) interpolation — used for
// the wow/flutter pitch wobble. Cubic matters: linear interpolation on a moving
// read-head dulls highs and aliases the warble.
class FractionalDelay
{
public:
    void prepare (double sampleRate, double maxDelayMs)
    {
        const int maxSamples = static_cast<int> (std::ceil (maxDelayMs * 0.001 * sampleRate)) + 4;
        size = std::max (maxSamples, 8);
        buffer.assign (static_cast<size_t> (size), 0.0f);
        writePos = 0;
    }
    void clear () { std::fill (buffer.begin(), buffer.end(), 0.0f); }
    float processSample (float in, float delaySamples) noexcept
    {
        buffer[static_cast<size_t> (writePos)] = in;
        delaySamples = std::clamp (delaySamples, 1.0f, static_cast<float> (size - 3));
        const float readPos = static_cast<float> (writePos) - delaySamples;
        int i1 = static_cast<int> (std::floor (readPos));
        const float frac = readPos - static_cast<float> (i1);
        auto wrap = [this] (int i) noexcept { i %= size; return i < 0 ? i + size : i; };
        const float y0 = buffer[static_cast<size_t> (wrap (i1 - 1))];
        const float y1 = buffer[static_cast<size_t> (wrap (i1    ))];
        const float y2 = buffer[static_cast<size_t> (wrap (i1 + 1))];
        const float y3 = buffer[static_cast<size_t> (wrap (i1 + 2))];
        const float a0 = -0.5f * y0 + 1.5f * y1 - 1.5f * y2 + 0.5f * y3;
        const float a1 =         y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float a2 = -0.5f * y0 + 0.5f * y2;
        const float a3 = y1;
        const float out = ((a0 * frac + a1) * frac + a2) * frac + a3;
        if (++writePos >= size) writePos = 0;
        return out;
    }
private:
    std::vector<float> buffer;
    int size = 0, writePos = 0;
};

struct TapeParams
{
    float driveDb      = 0.0f;
    float bias         = 0.15f;
    float wowDepthMs   = 3.0f;
    float wowRateHz    = 0.7f;
    float flutterDepthMs = 0.6f;
    float flutterRateHz  = 8.0f;
    float toneHz       = 6500.0f;
    float hiss         = 0.06f;
    float mix          = 1.0f;
    float outDb        = 0.0f;
};

class TapeChannel
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        baseDelayMs = 12.0;
        delay.prepare (sr, baseDelayMs + 30.0);
        delay.clear();
        for (auto* s : { &sDrive, &sBias, &sWowDepth, &sWowRate, &sFlutDepth,
                         &sFlutRate, &sTone, &sHiss, &sMix, &sOut })
            s->reset (sr, 0.02);
        wowPhase = flutPhase = 0.0;
        lpState = hissLpState = driftState = 0.0f;
        rng.seed (0xC0FFEEu);
    }
    void reset()
    {
        delay.clear();
        lpState = hissLpState = driftState = 0.0f;
        wowPhase = flutPhase = 0.0;
    }
    void setParams (const TapeParams& p)
    {
        sDrive.setTarget (p.driveDb);      sBias.setTarget (p.bias);
        sWowDepth.setTarget (p.wowDepthMs); sWowRate.setTarget (p.wowRateHz);
        sFlutDepth.setTarget (p.flutterDepthMs); sFlutRate.setTarget (p.flutterRateHz);
        sTone.setTarget (p.toneHz);        sHiss.setTarget (p.hiss);
        sMix.setTarget (p.mix);            sOut.setTarget (p.outDb);
    }
    void snapParams (const TapeParams& p)
    {
        sDrive.snap (p.driveDb);       sBias.snap (p.bias);
        sWowDepth.snap (p.wowDepthMs); sWowRate.snap (p.wowRateHz);
        sFlutDepth.snap (p.flutterDepthMs); sFlutRate.snap (p.flutterRateHz);
        sTone.snap (p.toneHz);         sHiss.snap (p.hiss);
        sMix.snap (p.mix);             sOut.snap (p.outDb);
    }
    float processSample (float x) noexcept
    {
        const float dry = x;
        // 1. drive + asymmetric tape saturation
        const float driveLin = dbToGain (sDrive.next());
        const float bias     = sBias.next();
        float s = x * driveLin;
        s = std::tanh (s + bias * 0.5f) - std::tanh (bias * 0.5f);
        s *= 1.0f / std::max (0.25f, driveLin * 0.7f);
        // 2. wow & flutter (modulated delay)
        const double twoPi = 6.283185307179586;
        const double wowRate  = std::max (0.01f, sWowRate.next());
        const double flutRate = std::max (0.1f,  sFlutRate.next());
        wowPhase  += twoPi * wowRate  / sampleRate;
        flutPhase += twoPi * flutRate / sampleRate;
        if (wowPhase  > twoPi) wowPhase  -= twoPi;
        if (flutPhase > twoPi) flutPhase -= twoPi;
        const float noise = dist (rng);
        driftState += 0.0006f * (noise - driftState);
        const float wowMs  = sWowDepth.next()  * (0.5f * (float) std::sin (wowPhase)  + 0.5f * driftState * 4.0f);
        const float flutMs = sFlutDepth.next() * (float) std::sin (flutPhase);
        const float delayMs = static_cast<float> (baseDelayMs) + wowMs + flutMs;
        const float delaySamples = delayMs * 0.001f * static_cast<float> (sampleRate);
        s = delay.processSample (s, delaySamples);
        // 3. tone: one-pole HF roll-off
        const float cutoff = std::clamp (sTone.next(), 500.0f, static_cast<float> (sampleRate * 0.45));
        const float g = 1.0f - std::exp (-twoPi * cutoff / static_cast<float> (sampleRate));
        lpState += g * (s - lpState);
        s = lpState;
        // 4. hiss: filtered noise, lightly program-dependent
        const float hissAmt = sHiss.next();
        if (hissAmt > 0.0f)
        {
            const float n = dist (rng);
            hissLpState += 0.05f * (n - hissLpState);
            const float bright = n - hissLpState;
            const float floorLvl = 0.25f + 0.75f * std::min (1.0f, std::abs (dry) * 4.0f);
            s += hissAmt * 0.05f * (0.7f * hissLpState + 0.3f * bright) * (0.5f + 0.5f * floorLvl);
        }
        // 5. mix + output
        const float mix = std::clamp (sMix.next(), 0.0f, 1.0f);
        float out = dry * (1.0f - mix) + s * mix;
        out *= dbToGain (sOut.next());
        return out;
    }
private:
    static float dbToGain (float db) noexcept { return std::pow (10.0f, db * 0.05f); }
    double sampleRate = 44100.0, baseDelayMs = 12.0;
    FractionalDelay delay;
    SmoothedValue sDrive, sBias, sWowDepth, sWowRate, sFlutDepth, sFlutRate, sTone, sHiss, sMix, sOut;
    double wowPhase = 0.0, flutPhase = 0.0;
    float lpState = 0.0f, hissLpState = 0.0f, driftState = 0.0f;
    std::mt19937 rng { 0xC0FFEEu };
    std::uniform_real_distribution<float> dist { -1.0f, 1.0f };
};

class TapeProcessor
{
public:
    void prepare (double sampleRate, int numChannels)
    {
        channels.resize (static_cast<size_t> (std::max (1, numChannels)));
        for (auto& c : channels) c.prepare (sampleRate);
    }
    void reset()          { for (auto& c : channels) c.reset(); }
    void setParams (const TapeParams& p)  { params = p; for (auto& c : channels) c.setParams (p); }
    void snapParams(const TapeParams& p)  { params = p; for (auto& c : channels) c.snapParams (p); }
    void process (float* const* buffer, int numChannels, int numSamples)
    {
        const int n = std::min (numChannels, static_cast<int> (channels.size()));
        for (int ch = 0; ch < n; ++ch)
        {
            float* data = buffer[ch];
            auto& c = channels[static_cast<size_t> (ch)];
            for (int i = 0; i < numSamples; ++i)
                data[i] = c.processSample (data[i]);
        }
    }
private:
    std::vector<TapeChannel> channels;
    TapeParams params;
};

} // namespace lizard
