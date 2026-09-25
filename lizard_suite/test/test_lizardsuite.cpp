// Standalone tests for the Lizard lo-fi suite — no JUCE, no framework.
//   g++ -std=c++17 -O2 -Wall test_lizardsuite.cpp -o t && ./t
#include "LizardVinyl.h"
#include "LizardMurk.h"
#include "LizardChew.h"
#include <cstdio>
#include <cmath>
#include <vector>

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (cond) { std::printf("  ok   %s\n", msg); } \
    else      { std::printf("  FAIL %s\n", msg); ++failures; } } while(0)

static bool finiteBuf(const std::vector<float>& v, float bound) {
    for (float x : v) if (!std::isfinite(x) || std::fabs(x) > bound) return false;
    return true;
}
static double rms(const std::vector<float>& v, int a, int b) {
    double s = 0; int nn = 0;
    for (int i = a; i < b && i < (int)v.size(); ++i) { s += double(v[i]) * v[i]; ++nn; }
    return nn ? std::sqrt(s / nn) : 0.0;
}
static std::vector<float> sine(int n, double f, double sr) {
    std::vector<float> v(n);
    for (int i = 0; i < n; ++i) v[i] = 0.6f * std::sin(2.0 * M_PI * f * i / sr);
    return v;
}

int main() {
    const double SR = 44100.0;
    std::printf("Lizard lo-fi suite tests\n");

    // ---------------- LizardVinyl ----------------
    std::printf("LizardVinyl\n");
    {
        const int N = 8192;
        // amount=0 -> bit-identical dry
        auto in = sine(N, 440, SR); auto x = in;
        LizardVinyl v; v.setHostSampleRate(SR); v.setAmount(0.0); v.setAge(1.0);
        v.process(x.data(), N);
        bool same = true; for (int i = 0; i < N; ++i) if (x[i] != in[i]) { same = false; break; }
        CHECK(same, "amount=0 is bit-identical dry");

        // silence in -> nonzero, finite bed
        std::vector<float> s(N, 0.0f);
        LizardVinyl v2; v2.setHostSampleRate(SR); v2.setAmount(1.0); v2.setAge(0.8); v2.setSeed(7);
        v2.process(s.data(), N);
        CHECK(rms(s, 0, N) > 0.0 && finiteBuf(s, 2.0f), "silence in -> finite nonzero vinyl bed");

        // higher age -> louder bed
        std::vector<float> lo(N, 0.0f), hi(N, 0.0f);
        LizardVinyl a; a.setHostSampleRate(SR); a.setAmount(1.0); a.setSeed(3); a.setAge(0.15);
        LizardVinyl b; b.setHostSampleRate(SR); b.setAmount(1.0); b.setSeed(3); b.setAge(0.95);
        a.process(lo.data(), N); b.process(hi.data(), N);
        std::printf("     age0.15 rms=%.5f  age0.95 rms=%.5f\n", rms(lo,0,N), rms(hi,0,N));
        CHECK(rms(hi,0,N) > rms(lo,0,N) * 1.5, "older record -> louder noise bed");

        // deterministic with the same seed
        std::vector<float> r1(N, 0.0f), r2(N, 0.0f);
        LizardVinyl c1; c1.setHostSampleRate(SR); c1.setAmount(1.0); c1.setAge(0.7); c1.setSeed(42);
        LizardVinyl c2; c2.setHostSampleRate(SR); c2.setAmount(1.0); c2.setAge(0.7); c2.setSeed(42);
        c1.process(r1.data(), N); c2.process(r2.data(), N);
        bool det = true; for (int i = 0; i < N; ++i) if (r1[i] != r2[i]) { det = false; break; }
        CHECK(det, "same seed -> identical output");
    }

    // ---------------- LizardMurk ----------------
    std::printf("LizardMurk\n");
    {
        const int N = 40000;
        // mix=0 -> dry identical
        auto in = sine(N, 300, SR); auto x = in;
        LizardMurk m; m.setHostSampleRate(SR); m.setMix(0.0);
        m.process(x.data(), N);
        bool same = true; for (int i = 0; i < N; ++i) if (x[i] != in[i]) { same = false; break; }
        CHECK(same, "mix=0 is bit-identical dry");

        // impulse -> a decaying tail that outlives the input
        std::vector<float> imp(N, 0.0f); imp[0] = 1.0f;
        LizardMurk r; r.setHostSampleRate(SR); r.setMix(1.0); r.setRoomSize(0.8); r.setDamp(0.5);
        r.process(imp.data(), N);
        double early = rms(imp, 2000, 8000), late = rms(imp, 24000, 32000);
        std::printf("     tail rms early=%.5f late=%.5f\n", early, late);
        CHECK(early > 0.0, "impulse produces a reverb tail after the input");
        CHECK(late < early && late >= 0.0, "the tail decays over time");
        CHECK(finiteBuf(imp, 4.0f), "reverb output stays finite & bounded");

        // stability at max room with sustained noise
        std::vector<float> nz(N);
        uint32_t s = 12345u;
        for (int i = 0; i < N; ++i) { s ^= s<<13; s ^= s>>17; s ^= s<<5; nz[i] = ((s&0xFFFF)/32768.0f - 1.0f) * 0.5f; }
        LizardMurk r2; r2.setHostSampleRate(SR); r2.setMix(1.0); r2.setRoomSize(1.0); r2.setDamp(0.2);
        r2.process(nz.data(), N);
        CHECK(finiteBuf(nz, 8.0f), "max room size stays stable (no runaway)");
    }

    // ---------------- LizardChew ----------------
    std::printf("LizardChew\n");
    {
        const int N = (int)(SR * 5); // 5 seconds
        // depth=0 -> dry identical
        auto in = sine(N, 220, SR); auto x = in;
        LizardChew c; c.setHostSampleRate(SR); c.setDepth(0.0); c.setRate(5.0);
        c.process(x.data(), N);
        bool same = true; for (int i = 0; i < N; ++i) if (x[i] != in[i]) { same = false; break; }
        CHECK(same, "depth=0 is bit-identical dry");

        // depth>0 on DC=1 -> dropouts pull gain below 1, never above ~1
        std::vector<float> dc(N, 1.0f);
        LizardChew c2; c2.setHostSampleRate(SR); c2.setDepth(0.8); c2.setRate(4.0); c2.setSeed(11);
        c2.process(dc.data(), N);
        float mn = 2.0f, mx = -2.0f;
        for (float g : dc) { mn = std::min(mn, g); mx = std::max(mx, g); }
        std::printf("     gain min=%.3f max=%.3f\n", mn, mx);
        CHECK(mn < 0.9f, "dropouts pull the gain well below unity");
        CHECK(mx <= 1.0001f, "gain never exceeds unity (never adds energy)");

        // |out| <= |in| on a real signal
        auto sg = sine(N, 330, SR); auto y = sg;
        LizardChew c3; c3.setHostSampleRate(SR); c3.setDepth(0.7); c3.setRate(6.0); c3.setSeed(5);
        c3.process(y.data(), N);
        bool bounded = true;
        for (int i = 0; i < N; ++i) if (std::fabs(y[i]) > std::fabs(sg[i]) + 1e-6f) { bounded = false; break; }
        CHECK(bounded, "output magnitude never exceeds input (pure gain)");

        // deterministic
        std::vector<float> a(N,1.0f), b(N,1.0f);
        LizardChew d1; d1.setHostSampleRate(SR); d1.setDepth(0.6); d1.setRate(5.0); d1.setSeed(77);
        LizardChew d2; d2.setHostSampleRate(SR); d2.setDepth(0.6); d2.setRate(5.0); d2.setSeed(77);
        d1.process(a.data(), N); d2.process(b.data(), N);
        bool det = true; for (int i = 0; i < N; ++i) if (a[i] != b[i]) { det = false; break; }
        CHECK(det, "same seed -> identical output");
    }

    std::printf("%s (%d failure%s)\n", failures ? "FAILURES" : "ALL PASS",
                failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
