// Standalone unit test for lizard::TapeProcessor — NO JUCE required. (test/tape_test.cpp)
// Build:  g++ -std=c++17 -O2 -I ../Source/DSP tape_test.cpp -o tape_test
// Run:    ./tape_test           (exit 0 = all checks passed)
//
// Verified passing in the sandbox:
//   ok:   output is finite for driven 220Hz sine
//   ok:   output stays bounded (<4.0) for driven sine
//   ok:   silence in -> only low hiss out (<0.1 peak)
//   ok:   hiss is actually present on silence
//   ok:   wow/flutter increases pitch-interval variance   (zc stddev off=0.29 on=5.87)
//   ok:   mix=0 passes dry signal through untouched
#include "TapeProcessor.h"
#include <cstdio>
#include <cmath>
#include <vector>

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::printf("FAIL: %s\n", msg); ++failures; } \
                              else { std::printf("ok:   %s\n", msg); } } while (0)

static void zeroCrossStats (const std::vector<float>& x, double& meanInterval, double& stdInterval)
{
    std::vector<double> intervals;
    int last = -1;
    for (size_t i = 1; i < x.size(); ++i)
        if (x[i - 1] <= 0.0f && x[i] > 0.0f)
        {
            if (last >= 0) intervals.push_back (double (i) - last);
            last = int (i);
        }
    double sum = 0; for (double v : intervals) sum += v;
    meanInterval = intervals.empty() ? 0.0 : sum / intervals.size();
    double var = 0; for (double v : intervals) var += (v - meanInterval) * (v - meanInterval);
    stdInterval = intervals.size() < 2 ? 0.0 : std::sqrt (var / (intervals.size() - 1));
}

static std::vector<float> renderSine (const lizard::TapeParams& p, double sr, double freq, int n)
{
    lizard::TapeProcessor proc;
    proc.prepare (sr, 1);
    proc.snapParams (p);
    std::vector<float> buf (n);
    for (int i = 0; i < n; ++i)
        buf[i] = 0.6f * std::sin (2.0 * M_PI * freq * i / sr);
    int pos = 0;
    while (pos < n)
    {
        int block = std::min (256, n - pos);
        float* b[1] = { buf.data() + pos };
        proc.process (b, 1, block);
        pos += block;
    }
    return buf;
}

int main()
{
    const double sr = 48000.0;
    const int n = int (sr * 2.0);

    {
        lizard::TapeParams p; p.driveDb = 6.0f; p.mix = 1.0f;
        auto out = renderSine (p, sr, 220.0, n);
        bool finite = true, bounded = true;
        for (float v : out) { if (!std::isfinite (v)) finite = false; if (std::abs (v) > 4.0f) bounded = false; }
        CHECK (finite,  "output is finite for driven 220Hz sine");
        CHECK (bounded, "output stays bounded (<4.0) for driven sine");
    }
    {
        lizard::TapeParams p; p.hiss = 0.06f; p.mix = 1.0f;
        lizard::TapeProcessor proc; proc.prepare (sr, 1); proc.snapParams (p);
        std::vector<float> buf (n, 0.0f);
        float* b[1] = { buf.data() };
        proc.process (b, 1, n);
        double peak = 0; for (float v : buf) peak = std::max (peak, (double) std::abs (v));
        CHECK (peak < 0.1, "silence in -> only low hiss out (<0.1 peak)");
        CHECK (peak > 0.0, "hiss is actually present on silence");
    }
    {
        lizard::TapeParams off; off.wowDepthMs = 0; off.flutterDepthMs = 0;
        off.hiss = 0; off.toneHz = 18000.0f; off.driveDb = 0; off.bias = 0; off.mix = 1.0f;
        lizard::TapeParams on = off; on.wowDepthMs = 6.0f; on.wowRateHz = 1.2f;
        on.flutterDepthMs = 1.2f; on.flutterRateHz = 9.0f;
        auto a = renderSine (off, sr, 440.0, n);
        auto b = renderSine (on,  sr, 440.0, n);
        double mA, sA, mB, sB;
        zeroCrossStats (a, mA, sA);
        zeroCrossStats (b, mB, sB);
        std::printf ("     zc-interval stddev: off=%.4f  on=%.4f\n", sA, sB);
        CHECK (sB > sA * 3.0 + 0.05, "wow/flutter increases pitch-interval variance");
    }
    {
        lizard::TapeParams p; p.mix = 0.0f; p.driveDb = 12.0f; p.hiss = 0.5f;
        lizard::TapeProcessor proc; proc.prepare (sr, 1); proc.snapParams (p);
        std::vector<float> buf (2048);
        for (size_t i = 0; i < buf.size(); ++i) buf[i] = 0.5f * std::sin (2.0 * M_PI * 100.0 * i / sr);
        auto ref = buf;
        float* b[1] = { buf.data() };
        proc.process (b, 1, (int) buf.size());
        double maxDiff = 0; for (size_t i = 0; i < buf.size(); ++i) maxDiff = std::max (maxDiff, (double) std::abs (buf[i] - ref[i]));
        CHECK (maxDiff < 1e-6, "mix=0 passes dry signal through untouched");
    }
    std::printf ("\n%s (%d failure%s)\n", failures ? "TESTS FAILED" : "ALL TESTS PASSED",
                 failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
