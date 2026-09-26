// =============================================================================
//  LizardDust.h  —  a header-only, JUCE-independent lo-fi "dusty sampler" core
//
//  Models the three artifacts that actually make old 12-bit samplers sound
//  dusty (see "Why Old Samplers Sound Dusty"), as a real-time, same-length,
//  mono block effect:
//
//    1. Sample-rate reduction by sample-&-hold  -> the missing reconstruction
//       filter: a staircase output whose zero-order hold leaves spectral IMAGES
//       above the reduced Nyquist. With a non-integer host/target ratio the
//       hold points land irregularly, which is the "complicated aliasing" the
//       Stanford SP-12 analysis blames for most of the character.
//    2. Bit-depth reduction (quantize, no dither) -> the 12-bit grain / noisy tail.
//    3. An optional gentle 1-pole low-pass with light drive -> the SSM-filter
//       colour on the filtered channels. Off by default, because the SP's
//       signature is the filter that ISN'T there.
//
//  True pitch-shift (the 45-rpm pitch-down trick) changes DURATION, so it lives
//  in the offline Python degrader, not in this real-time core.
//
//  No dependencies beyond <cmath>/<algorithm>. Drop into a JUCE processor and
//  call process() from processBlock (per channel).
// =============================================================================
#pragma once
#include <cmath>
#include <algorithm>

class LizardDust
{
public:
    // ---- configuration -------------------------------------------------------
    void  setHostSampleRate (double sr) noexcept { hostRate_ = (sr > 0.0 ? sr : 44100.0); }

    // Target "sampler" rate in Hz (e.g. 26040 for an SP-1200 feel). Values >=
    // host rate disable the reducer (ratio clamped to 1).
    void  setTargetRate (double hz) noexcept { targetRate_ = (hz > 0.0 ? hz : hostRate_); }

    // Bit depth 2..16. 16 = effectively clean.
    void  setBitDepth (int bits) noexcept { bits_ = std::clamp(bits, 2, 16); }

    // Gentle low-pass (SSM flavour). cutoff in Hz; <=0 or >=host/2 disables it.
    void  setLowpassHz (double hz) noexcept { lpHz_ = hz; }

    // Pre-quantise drive (1 = unity). Adds a little tanh warmth/dirt.
    void  setDrive (double d) noexcept { drive_ = std::max(0.0001, d); }

    // Dry/wet, 0..1.
    void  setMix (double m) noexcept { mix_ = std::clamp(m, 0.0, 1.0); }

    void  reset() noexcept
    {
        hold_ = 0.0f; phase_ = 1.0; lpZ_ = 0.0f;
    }

    // ---- processing ----------------------------------------------------------
    // Processes n samples of x in place.
    void process (float* x, int n) noexcept
    {
        const double ratio  = std::clamp(targetRate_ / hostRate_, 1.0e-4, 1.0);
        const float  levels = static_cast<float>((1 << bits_) - 1);       // e.g. 12-bit -> 4095
        const float  invLev = 1.0f / levels;
        const bool   useLp  = (lpHz_ > 0.0 && lpHz_ < hostRate_ * 0.5);
        // one-pole coefficient
        const float  a = useLp
            ? static_cast<float>(std::exp(-2.0 * M_PI * (lpHz_ / hostRate_)))
            : 0.0f;

        for (int i = 0; i < n; ++i)
        {
            const float dry = x[i];

            // 1) sample-&-hold rate reduction (imaging + irregular aliasing)
            phase_ += ratio;
            if (phase_ >= 1.0) { phase_ -= 1.0; hold_ = dry; }
            float s = hold_;

            // 2) drive
            if (drive_ != 1.0)
                s = std::tanh(s * static_cast<float>(drive_)) / std::tanh(static_cast<float>(drive_));

            // 3) bit-depth quantise (mid-tread, no dither), clamped to [-1,1]
            float c = std::clamp(s, -1.0f, 1.0f);
            c = std::round((c * 0.5f + 0.5f) * levels) * invLev * 2.0f - 1.0f;

            // 4) optional SSM-flavour low-pass
            if (useLp)
            {
                lpZ_ = (1.0f - a) * c + a * lpZ_;
                c = lpZ_;
            }

            // 5) mix
            x[i] = dry + (c - dry) * static_cast<float>(mix_);
        }
    }

private:
    double hostRate_   = 44100.0;
    double targetRate_ = 26040.0;   // SP-1200-ish
    int    bits_       = 12;
    double lpHz_       = 0.0;       // off
    double drive_      = 1.0;
    double mix_        = 1.0;

    float  hold_  = 0.0f;
    double phase_ = 1.0;            // >=1 so the first sample latches
    float  lpZ_   = 0.0f;
};
