// =============================================================================
//  LizardChew.h  —  worn-tape dropout / azimuth wobble (header-only, JUCE-free)
//
//  Models a chewed-up cassette: the level randomly sags as the tape loses
//  contact with the head, then recovers. A per-sample chance starts a dropout
//  to a random depth; the target relaxes back toward unity and a smoothing
//  filter keeps the gain envelope from clicking. Purely a gain modulation, so
//  |out| <= |in| and it can never add energy. Deterministic (seedable).
// =============================================================================
#pragma once
#include <cmath>
#include <algorithm>
#include <cstdint>

class LizardChew
{
public:
    void setHostSampleRate (double sr) noexcept { sr_ = (sr > 0.0 ? sr : 44100.0); }
    void setRate    (double evPerSec) noexcept { rate_    = std::max(0.0, evPerSec); } // dropouts/sec
    void setDepth   (double d)        noexcept { depth_   = std::clamp(d, 0.0, 1.0); } // 0 = off
    void setSmoothMs(double ms)       noexcept { smoothMs_= std::max(0.1, ms); }
    void setSeed    (uint32_t s)      noexcept { rng_ = (s ? s : 1u); }
    void reset() noexcept { gain_ = 1.0f; target_ = 1.0f; }

    void process (float* x, int n) noexcept
    {
        if (depth_ <= 0.0) return;                          // fully dry, bit-identical
        const double p   = rate_ / sr_;                     // per-sample dropout probability
        const float  sm  = static_cast<float>(std::exp(-1.0 / (0.001 * smoothMs_ * sr_)));
        const float  rec = static_cast<float>(std::exp(-1.0 / (0.05 * sr_)));   // ~50 ms recovery

        for (int i = 0; i < n; ++i)
        {
            if (frand() < p)
                target_ = 1.0f - static_cast<float>(depth_) * frand();  // sag to a random depth
            target_ = 1.0f + (target_ - 1.0f) * rec;         // relax back toward unity
            gain_   = target_ + (gain_ - target_) * sm;      // click-free smoothing
            x[i] *= gain_;
        }
    }

private:
    float frand() noexcept
    {
        rng_ ^= rng_ << 13; rng_ ^= rng_ >> 17; rng_ ^= rng_ << 5;
        return static_cast<float>(rng_ & 0xFFFFFF) / static_cast<float>(0x1000000);
    }

    double   sr_ = 44100.0, rate_ = 3.0, depth_ = 0.5, smoothMs_ = 8.0;
    uint32_t rng_ = 99991u;
    float    gain_ = 1.0f, target_ = 1.0f;
};
