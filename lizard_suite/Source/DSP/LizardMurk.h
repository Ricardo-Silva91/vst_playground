// =============================================================================
//  LizardMurk.h  —  a dark, damped lo-fi reverb (header-only, JUCE-free)
//
//  A compact Schroeder/Freeverb-style reverb (4 damped comb filters in
//  parallel into 2 allpass diffusers) tuned for the "basement / underwater"
//  space rather than a pristine hall: the damping low-pass in each comb's
//  feedback eats the top end so tails go murky. Allocates its delay lines in
//  the constructor / setHostSampleRate (never in process()).
// =============================================================================
#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

class LizardMurk
{
public:
    LizardMurk() { rebuild(); }

    void setHostSampleRate (double sr) noexcept { sr_ = (sr > 0.0 ? sr : 44100.0); rebuild(); }
    void setRoomSize (double r) noexcept { room_ = std::clamp(r, 0.0, 1.0); }   // tail length
    void setDamp     (double d) noexcept { damp_ = std::clamp(d, 0.0, 1.0); }   // darkness
    void setMix      (double m) noexcept { mix_  = std::clamp(m, 0.0, 1.0); }
    void reset() noexcept { for (auto& c : combs_) c.reset(); for (auto& a : alls_) a.reset(); }

    void process (float* x, int n) noexcept
    {
        if (mix_ <= 0.0) return;                            // fully dry, bit-identical
        const float fb  = 0.70f + 0.28f * static_cast<float>(room_);   // <=0.98, stable
        const float dmp = 0.20f + 0.75f * static_cast<float>(damp_);
        for (auto& c : combs_) c.dmp = dmp;

        for (int i = 0; i < n; ++i)
        {
            const float in = x[i] * 0.20f;                  // input gain into the network
            float acc = 0.0f;
            for (auto& c : combs_) acc += c.process(in, fb);
            for (auto& a : alls_)  acc = a.process(acc);
            x[i] = x[i] + (acc - x[i]) * static_cast<float>(mix_);
        }
    }

private:
    struct Comb {
        std::vector<float> buf; int idx = 0; float store = 0.0f, dmp = 0.5f;
        void size (int nn) { buf.assign(std::max(1, nn), 0.0f); idx = 0; store = 0.0f; }
        void reset()       { std::fill(buf.begin(), buf.end(), 0.0f); idx = 0; store = 0.0f; }
        float process (float in, float fb) {
            const float y = buf[idx];
            store = y * (1.0f - dmp) + store * dmp;         // damping low-pass in feedback
            buf[idx] = in + store * fb;
            if (++idx >= static_cast<int>(buf.size())) idx = 0;
            return y;
        }
    };
    struct Allpass {
        std::vector<float> buf; int idx = 0;
        void size (int nn) { buf.assign(std::max(1, nn), 0.0f); idx = 0; }
        void reset()       { std::fill(buf.begin(), buf.end(), 0.0f); idx = 0; }
        float process (float in) {
            const float y = buf[idx];
            const float out = -in + y;
            buf[idx] = in + y * 0.5f;
            if (++idx >= static_cast<int>(buf.size())) idx = 0;
            return out;
        }
    };
    void rebuild()
    {
        const double k = sr_ / 44100.0;
        const int cl[4] = {1116, 1188, 1277, 1356};
        const int al[2] = {556, 441};
        combs_.assign(4, Comb()); alls_.assign(2, Allpass());
        for (int i = 0; i < 4; ++i) combs_[i].size(static_cast<int>(std::round(cl[i] * k)));
        for (int i = 0; i < 2; ++i) alls_[i].size(static_cast<int>(std::round(al[i] * k)));
    }

    double sr_ = 44100.0, room_ = 0.6, damp_ = 0.5, mix_ = 0.3;
    std::vector<Comb> combs_;
    std::vector<Allpass> alls_;
};
