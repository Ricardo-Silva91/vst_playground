// Standalone unit test for LizardDust.h — no JUCE, no framework.
//   g++ -std=c++17 -O2 test_lizarddust.cpp -o t && ./t
#include "LizardDust.h"
#include <cstdio>
#include <cmath>
#include <vector>
#include <set>

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (cond) { std::printf("  ok   %s\n", msg); } \
    else      { std::printf("  FAIL %s\n", msg); ++failures; } } while(0)

static bool finiteBuf(const std::vector<float>& v, float bound) {
    for (float x : v) if (!std::isfinite(x) || std::fabs(x) > bound) return false;
    return true;
}
static double rms(const std::vector<float>& v) {
    double s = 0; for (float x : v) s += double(x) * x; return std::sqrt(s / v.size());
}
static std::vector<float> sine(int n, double f, double sr) {
    std::vector<float> v(n);
    for (int i = 0; i < n; ++i) v[i] = 0.9f * std::sin(2.0 * M_PI * f * i / sr);
    return v;
}

int main() {
    const double SR = 44100.0;
    const int    N  = 4096;
    std::printf("LizardDust tests\n");

    // 1) Dry passthrough: mix = 0 must be bit-identical to input.
    {
        auto in = sine(N, 1000, SR);
        auto x = in;
        LizardDust d; d.setHostSampleRate(SR); d.setMix(0.0);
        d.setBitDepth(4); d.setTargetRate(8000); d.setDrive(3.0); // harsh but bypassed
        d.process(x.data(), N);
        bool identical = true;
        for (int i = 0; i < N; ++i) if (x[i] != in[i]) { identical = false; break; }
        CHECK(identical, "mix=0 is bit-identical dry passthrough");
    }

    // 2) Finite / bounded under extreme settings.
    {
        auto x = sine(N, 12000, SR);
        LizardDust d; d.setHostSampleRate(SR); d.setMix(1.0);
        d.setBitDepth(2); d.setTargetRate(6000); d.setDrive(8.0); d.setLowpassHz(3000);
        d.process(x.data(), N);
        CHECK(finiteBuf(x, 1.5f), "finite & bounded under extreme crush/reduce/drive");
    }

    // 3) Bit-depth quantise: 3-bit output takes few distinct levels.
    {
        auto x = sine(N, 220, SR);
        LizardDust d; d.setHostSampleRate(SR); d.setMix(1.0);
        d.setBitDepth(3); d.setTargetRate(SR); // no SR reduction, isolate the quantiser
        d.process(x.data(), N);
        std::set<float> levels(x.begin(), x.end());
        // 3 bits -> up to 9 mid-tread codes across the signal's range
        CHECK(levels.size() <= 12 && levels.size() >= 3,
              "3-bit crush collapses to a small set of levels");
    }

    // 4) Sample-&-hold: a ramp becomes a staircase (runs of repeated values).
    {
        std::vector<float> x(N);
        for (int i = 0; i < N; ++i) x[i] = -0.9f + 1.8f * (float(i) / (N - 1));
        LizardDust d; d.setHostSampleRate(SR); d.setMix(1.0);
        d.setBitDepth(16); d.setTargetRate(SR / 4.0); // ~4x decimation
        d.process(x.data(), N);
        std::set<float> uniq(x.begin(), x.end());
        int adjacentEqual = 0;
        for (int i = 1; i < N; ++i) if (x[i] == x[i-1]) ++adjacentEqual;
        CHECK(uniq.size() < size_t(N) * 0.4 && uniq.size() > size_t(N) * 0.15,
              "4x sample-&-hold reduces unique values ~4x");
        CHECK(adjacentEqual > N / 2, "sample-&-hold output is piecewise-constant");
    }

    // 5) Imaging: a high sine is altered far more by SR reduction than a low sine.
    {
        auto hi_in = sine(N, 11000, SR);
        auto lo_in = sine(N, 200, SR);
        auto hi = hi_in, lo = lo_in;
        LizardDust d; d.setHostSampleRate(SR); d.setMix(1.0);
        d.setBitDepth(16); d.setTargetRate(13000); // reduce below 22.05k Nyquist
        LizardDust d2 = d;
        d.process(hi.data(), N);
        d2.process(lo.data(), N);
        std::vector<float> hiErr(N), loErr(N);
        for (int i = 0; i < N; ++i) { hiErr[i] = hi[i] - hi_in[i]; loErr[i] = lo[i] - lo_in[i]; }
        double hiE = rms(hiErr), loE = rms(loErr);
        std::printf("     high-sine reduction error=%.4f  low-sine=%.4f\n", hiE, loE);
        CHECK(hiE > loE * 2.0, "rate reduction adds far more artifact energy to highs (imaging)");
    }

    std::printf("%s (%d failure%s)\n", failures ? "FAILURES" : "ALL PASS",
                failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
