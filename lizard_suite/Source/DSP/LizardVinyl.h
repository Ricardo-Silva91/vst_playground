// =============================================================================
//  LizardVinyl.h  —  procedural vinyl-record noise bed (header-only, JUCE-free)
//
//  Adds the sound of the *medium* rather than the sampler: age-controlled
//  surface hiss, low turntable rumble, and sparse decaying crackle/pops. This
//  is the "the source, not the sampler" item from the dusty-sampler essay — the
//  highest-bang-for-buck lo-fi ingredient. Additive: it lays a noise bed over
//  the dry signal, scaled by `amount`. Deterministic (seedable) so it tests.
// =============================================================================
#pragma once
#include <cmath>
#include <algorithm>
#include <cstdint>

class LizardVinyl
{
public:
    void setHostSampleRate (double sr) noexcept { sr_ = (sr > 0.0 ? sr : 44100.0); }
    void setAge   (double a) noexcept { age_    = std::clamp(a, 0.0, 1.0); }  // 0 pristine .. 1 trashed
    void setAmount(double m) noexcept { amount_ = std::clamp(m, 0.0, 1.0); }  // bed level (0 = off)
    void setSeed  (uint32_t s) noexcept { rng_ = (s ? s : 1u); }
    void reset() noexcept { hissZ_ = rumbleZ_ = clickAmp_ = 0.0f; }

    void process (float* x, int n) noexcept
    {
        if (amount_ <= 0.0) return;                        // fully dry, bit-identical
        const double hissLvl   = 0.0008 + 0.006 * age_;
        const double rumbleLvl = 0.002  + 0.02  * age_;
        const double clickProb = (0.0006 + 0.02 * age_) * (sr_ / 44100.0) * 0.5;
        const float  hissA     = 0.6f;                                        // hiss tone
        const float  rumbleA   = static_cast<float>(std::exp(-2.0 * M_PI * (50.0 / sr_))); // ~50 Hz

        for (int i = 0; i < n; ++i)
        {
            const float w1 = frand() * 2.0f - 1.0f;
            hissZ_ = (1.0f - hissA) * w1 + hissA * hissZ_;
            const float hiss = hissZ_ * static_cast<float>(hissLvl);

            const float w2 = frand() * 2.0f - 1.0f;
            rumbleZ_ = (1.0f - rumbleA) * w2 + rumbleA * rumbleZ_;
            const float rumble = rumbleZ_ * static_cast<float>(rumbleLvl) * 4.0f;

            if (frand() < clickProb)
                clickAmp_ = (frand() * 2.0f - 1.0f) * (0.2f + 0.8f * static_cast<float>(age_));
            const float click = clickAmp_;
            clickAmp_ *= 0.6f;                              // fast click decay

            x[i] += (hiss + rumble + click) * static_cast<float>(amount_);
        }
    }

private:
    // xorshift32 -> [0,1)
    float frand() noexcept
    {
        rng_ ^= rng_ << 13; rng_ ^= rng_ >> 17; rng_ ^= rng_ << 5;
        return static_cast<float>(rng_ & 0xFFFFFF) / static_cast<float>(0x1000000);
    }

    double   sr_ = 44100.0, age_ = 0.4, amount_ = 0.5;
    uint32_t rng_ = 22222u;
    float    hissZ_ = 0.0f, rumbleZ_ = 0.0f, clickAmp_ = 0.0f;
};
