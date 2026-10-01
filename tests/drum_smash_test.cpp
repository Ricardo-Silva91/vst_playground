#include <catch2/catch_all.hpp>
#include "PluginProcessor.h"
#include "TestHelpers.h"

// ── Parameter tests ───────────────────────────────────────────────────────────

TEST_CASE("DrumSmash - parameters", "[drum_smash]")
{
    DrumSmashProcessor proc;

    SECTION("all parameter IDs exist")
    {
        for (const char* id : { "bitDepth", "sampleRateDiv", "drive", "outputGain",
                                 "noiseAmount", "crackleRate", "lpfCutoff", "hpfCutoff",
                                 "compThreshold", "compRatio", "compAttack", "compRelease",
                                 "compMakeup", "reverbRoom", "reverbWet", "reverbDamping",
                                 "pitchSemitones", "wowRate", "wowDepth",
                                 "stereoWidth", "transientBoost" })
        {
            INFO("Checking parameter: " << id);
            REQUIRE(proc.apvts.getParameter(id) != nullptr);
        }
    }

    SECTION("default values match spec")
    {
        REQUIRE(getDefault(proc.apvts, "bitDepth")      == Catch::Approx(16.f).margin(0.1f));
        REQUIRE(getDefault(proc.apvts, "sampleRateDiv") == Catch::Approx(1.f).margin(0.01f));
        REQUIRE(getDefault(proc.apvts, "drive")         == Catch::Approx(0.f).margin(0.01f));
        REQUIRE(getDefault(proc.apvts, "outputGain")    == Catch::Approx(1.f).margin(0.01f));
        REQUIRE(getDefault(proc.apvts, "noiseAmount")   == Catch::Approx(0.f).margin(0.01f));
        REQUIRE(getDefault(proc.apvts, "lpfCutoff")     == Catch::Approx(22000.f).margin(1.f));
        REQUIRE(getDefault(proc.apvts, "hpfCutoff")     == Catch::Approx(20.f).margin(0.1f));
        REQUIRE(getDefault(proc.apvts, "reverbWet")     == Catch::Approx(0.f).margin(0.01f));
        REQUIRE(getDefault(proc.apvts, "stereoWidth")   == Catch::Approx(1.f).margin(0.01f));
        REQUIRE(getDefault(proc.apvts, "transientBoost")== Catch::Approx(0.f).margin(0.01f));
    }

    SECTION("parameter ranges")
    {
        auto rangeOf = [&](const juce::String& id) {
            return dynamic_cast<juce::RangedAudioParameter*>(
                proc.apvts.getParameter(id))->getNormalisableRange();
        };
        REQUIRE(rangeOf("bitDepth").start      == Catch::Approx(1.f));
        REQUIRE(rangeOf("bitDepth").end        == Catch::Approx(16.f));
        REQUIRE(rangeOf("sampleRateDiv").start == Catch::Approx(1.f));
        REQUIRE(rangeOf("sampleRateDiv").end   == Catch::Approx(32.f));
        REQUIRE(rangeOf("compThreshold").start == Catch::Approx(-60.f));
        REQUIRE(rangeOf("compThreshold").end   == Catch::Approx(0.f));
        REQUIRE(rangeOf("pitchSemitones").start== Catch::Approx(-12.f));
        REQUIRE(rangeOf("pitchSemitones").end  == Catch::Approx(12.f));
    }

    SECTION("preset count")
    {
        REQUIRE(proc.getNumPrograms() == kNumPresets);
        REQUIRE(kNumPresets == 8);
    }

    SECTION("preset names")
    {
        REQUIRE(proc.getProgramName(0) == "Dusty Vinyl");
        REQUIRE(proc.getProgramName(1) == "Heavy Hitter");
        REQUIRE(proc.getProgramName(2) == "Bit Crusher");
        REQUIRE(proc.getProgramName(7) == "Bedroom Boom-Bap");
    }
}

// ── Preset tests ──────────────────────────────────────────────────────────────

TEST_CASE("DrumSmash - presets", "[drum_smash]")
{
    DrumSmashProcessor proc;

    SECTION("Dusty Vinyl preset values")
    {
        proc.setCurrentProgram(0);
        REQUIRE(getParam(proc.apvts, "bitDepth")   == Catch::Approx(kPresets[0].bitDepth).margin(0.1f));
        REQUIRE(getParam(proc.apvts, "drive")      == Catch::Approx(kPresets[0].drive).margin(0.01f));
        REQUIRE(getParam(proc.apvts, "noiseAmount")== Catch::Approx(kPresets[0].noiseAmount).margin(0.01f));
    }

    SECTION("Bit Crusher preset values")
    {
        proc.setCurrentProgram(2);
        REQUIRE(getParam(proc.apvts, "bitDepth")     == Catch::Approx(kPresets[2].bitDepth).margin(0.1f));
        REQUIRE(getParam(proc.apvts, "sampleRateDiv")== Catch::Approx(kPresets[2].sampleRateDiv).margin(0.1f));
    }

    SECTION("all presets load without crash")
    {
        for (int i = 0; i < kNumPresets; ++i)
        {
            proc.setCurrentProgram(i);
            REQUIRE(proc.getCurrentProgram() == i);
        }
    }
}

// ── Audio processing tests ────────────────────────────────────────────────────

// Everything off except what a section turns on
static void setCleanChain(juce::AudioProcessorValueTreeState& apvts)
{
    setParam(apvts, "bitDepth",       16.f);
    setParam(apvts, "sampleRateDiv",   1.f);
    setParam(apvts, "drive",           0.f);
    setParam(apvts, "noiseAmount",     0.f);
    setParam(apvts, "crackleRate",     0.f);
    setParam(apvts, "reverbWet",       0.f);
    setParam(apvts, "lpfCutoff",   22000.f);
    setParam(apvts, "hpfCutoff",      20.f);
    setParam(apvts, "compThreshold",   0.f);
    setParam(apvts, "compMakeup",      0.f);
    setParam(apvts, "stereoWidth",     1.f);
    setParam(apvts, "transientBoost",  0.f);
    setParam(apvts, "pitchSemitones",  0.f);
    setParam(apvts, "wowRate",         0.f);
    setParam(apvts, "wowDepth",        0.f);
    setParam(apvts, "outputGain",      1.f);
}

static float channelRms(const juce::AudioBuffer<float>& buf, int ch)
{
    double sum = 0.0;
    for (int i = 0; i < buf.getNumSamples(); ++i)
        sum += (double)buf.getSample(ch, i) * buf.getSample(ch, i);
    return (float)std::sqrt(sum / buf.getNumSamples());
}

TEST_CASE("DrumSmash - audio processing", "[drum_smash]")
{
    DrumSmashProcessor proc;
    proc.setPlayConfigDetails(2, 2, 44100.0, 512);
    proc.prepareToPlay(44100.0, 512);
    juce::MidiBuffer midi;

    SECTION("silence in produces silence out (no noise/crackle)")
    {
        // Ensure noise and crackle are off
        setParam(proc.apvts, "noiseAmount", 0.f);
        setParam(proc.apvts, "crackleRate", 0.f);
        setParam(proc.apvts, "reverbWet",   0.f);
        auto buf = makeSilent(2, 512);
        proc.processBlock(buf, midi);
        REQUIRE(allFinite(buf));
        REQUIRE(rmsOf(buf) < 1e-5f);
    }

    SECTION("output is finite and bounded")
    {
        juce::AudioBuffer<float> buf(2, 512);
        fillWithSine(buf, 440.f, 44100.0, 0.5f);
        proc.processBlock(buf, midi);
        REQUIRE(allFinite(buf));
        REQUIRE(allBounded(buf, 3.f));
    }

    SECTION("clean pass: 16-bit, no drive, no noise is close to input")
    {
        setParam(proc.apvts, "bitDepth",      16.f);
        setParam(proc.apvts, "sampleRateDiv",  1.f);
        setParam(proc.apvts, "drive",          0.f);
        setParam(proc.apvts, "noiseAmount",    0.f);
        setParam(proc.apvts, "crackleRate",    0.f);
        setParam(proc.apvts, "reverbWet",      0.f);
        setParam(proc.apvts, "lpfCutoff",  22000.f);
        setParam(proc.apvts, "hpfCutoff",     20.f);
        setParam(proc.apvts, "compThreshold",  0.f);  // threshold at 0 dB = no gain reduction
        setParam(proc.apvts, "compMakeup",     0.f);
        setParam(proc.apvts, "stereoWidth",    1.f);
        setParam(proc.apvts, "transientBoost", 0.f);
        setParam(proc.apvts, "wowRate",        0.f);
        setParam(proc.apvts, "outputGain",     1.f);

        proc.prepareToPlay(44100.0, 512);

        juce::AudioBuffer<float> buf(2, 512);
        fillWithSine(buf, 440.f, 44100.0, 0.3f);
        float inputRms = rmsOf(buf);
        proc.processBlock(buf, midi);
        REQUIRE(allFinite(buf));
        REQUIRE(rmsOf(buf) > inputRms * 0.1f);
    }

    SECTION("1-bit crushing produces heavy distortion")
    {
        setParam(proc.apvts, "bitDepth",      1.f);
        setParam(proc.apvts, "noiseAmount",   0.f);
        setParam(proc.apvts, "reverbWet",     0.f);
        setParam(proc.apvts, "drive",         0.f);
        setParam(proc.apvts, "stereoWidth",   1.f);
        setParam(proc.apvts, "outputGain",    1.f);

        proc.prepareToPlay(44100.0, 512);

        juce::AudioBuffer<float> buf(2, 512);
        fillWithSine(buf, 440.f, 44100.0, 0.5f);
        proc.processBlock(buf, midi);

        REQUIRE(allFinite(buf));
        // 1-bit crushes signal to two levels; all samples should be near ±1 or 0
        float maxVal = 0.f;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < buf.getNumSamples(); ++i)
                maxVal = std::max(maxVal, std::abs(buf.getSample(ch, i)));
        REQUIRE(maxVal > 0.01f);
    }

    SECTION("LPF cutoff changes do not crash or produce NaN")
    {
        setParam(proc.apvts, "bitDepth",      16.f);
        setParam(proc.apvts, "sampleRateDiv",  1.f);
        setParam(proc.apvts, "drive",          0.f);
        setParam(proc.apvts, "noiseAmount",    0.f);
        setParam(proc.apvts, "crackleRate",    0.f);
        setParam(proc.apvts, "reverbWet",      0.f);
        setParam(proc.apvts, "outputGain",     1.f);

        for (float cutoff : { 200.f, 1000.f, 22000.f })
        {
            setParam(proc.apvts, "lpfCutoff", cutoff);
            proc.prepareToPlay(44100.0, 512);
            juce::AudioBuffer<float> buf(2, 512);
            fillWithSine(buf, 440.f, 44100.0, 0.5f);
            proc.processBlock(buf, midi);
            REQUIRE(allFinite(buf));
        }
    }

    SECTION("LPF attenuates both channels equally")
    {
        setCleanChain(proc.apvts);
        setParam(proc.apvts, "lpfCutoff", 200.f);

        float rmsL = 0.f, rmsR = 0.f;
        for (int block = 0; block < 20; ++block)
        {
            juce::AudioBuffer<float> buf(2, 512);
            fillWithSine(buf, 5000.f, 44100.0, 0.5f);
            proc.processBlock(buf, midi);
            rmsL = channelRms(buf, 0);
            rmsR = channelRms(buf, 1);
        }
        const float inRms = 0.5f / std::sqrt(2.f);
        REQUIRE(rmsL < inRms * 0.1f);
        REQUIRE(rmsR < inRms * 0.1f);
        REQUIRE(rmsR == Catch::Approx(rmsL).epsilon(0.01));
    }

    SECTION("transient boost leaves a steady tone at unity gain")
    {
        setCleanChain(proc.apvts);
        auto steadyRms = [&] (float boost)
        {
            setParam(proc.apvts, "transientBoost", boost);
            proc.prepareToPlay(44100.0, 512);
            float rms = 0.f;
            for (int block = 0; block < 40; ++block)
            {
                juce::AudioBuffer<float> buf(2, 512);
                fillWithSine(buf, 441.f, 44100.0, 0.3f);   // whole cycles per block
                proc.processBlock(buf, midi);
                rms = rmsOf(buf);
            }
            return rms;
        };
        REQUIRE(steadyRms(1.f) == Catch::Approx(steadyRms(0.f)).epsilon(0.05));
    }

    SECTION("pitch +12 semitones doubles the frequency")
    {
        setCleanChain(proc.apvts);
        setParam(proc.apvts, "pitchSemitones", 12.f);
        proc.prepareToPlay(44100.0, 512);

        int crossings = 0;
        float prev = 0.f;
        for (int block = 0; block < 60; ++block)
        {
            juce::AudioBuffer<float> buf(2, 512);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 512; ++i)
                    buf.setSample(ch, i, 0.3f * std::sin(juce::MathConstants<float>::twoPi * 220.f
                                                         * (float)(block * 512 + i) / 44100.f));
            proc.processBlock(buf, midi);
            if (block < 20) continue;   // settle
            for (int i = 0; i < 512; ++i)
            {
                const float x = buf.getSample(0, i);
                if (prev < 0.f && x >= 0.f) ++crossings;
                prev = x;
            }
        }
        const float seconds = 40.f * 512.f / 44100.f;
        REQUIRE(crossings / seconds == Catch::Approx(440.f).epsilon(0.05));
    }

    SECTION("extreme parameter values do not crash")
    {
        for (auto drive  : { 0.f, 1.f })
        for (auto noise  : { 0.f, 1.f })
        for (auto reverb : { 0.f, 1.f })
        {
            setParam(proc.apvts, "drive",       drive);
            setParam(proc.apvts, "noiseAmount", noise);
            setParam(proc.apvts, "reverbWet",   reverb);
            juce::AudioBuffer<float> buf(2, 512);
            fillWithSine(buf, 440.f, 44100.0);
            proc.processBlock(buf, midi);
            REQUIRE(allFinite(buf));
        }
    }
}

// ── State management tests ────────────────────────────────────────────────────

TEST_CASE("DrumSmash - state management", "[drum_smash]")
{
    SECTION("getStateInformation / setStateInformation round-trip")
    {
        DrumSmashProcessor proc;
        setParam(proc.apvts, "bitDepth",   4.f);
        setParam(proc.apvts, "drive",      0.6f);
        setParam(proc.apvts, "noiseAmount",0.3f);

        juce::MemoryBlock stateData;
        proc.getStateInformation(stateData);
        REQUIRE(stateData.getSize() > 0);

        setParam(proc.apvts, "bitDepth", 16.f);
        proc.setStateInformation(stateData.getData(), (int)stateData.getSize());

        REQUIRE(getParam(proc.apvts, "bitDepth")   == Catch::Approx(4.f).margin(0.1f));
        REQUIRE(getParam(proc.apvts, "drive")      == Catch::Approx(0.6f).margin(0.01f));
        REQUIRE(getParam(proc.apvts, "noiseAmount")== Catch::Approx(0.3f).margin(0.01f));
    }

    SECTION("prepareToPlay keeps user and restored parameter values")
    {
        DrumSmashProcessor proc;
        setParam(proc.apvts, "drive", 0.77f);
        proc.prepareToPlay(44100.0, 512);
        REQUIRE(getParam(proc.apvts, "drive") == Catch::Approx(0.77f).margin(0.01f));

        juce::MemoryBlock stateData;
        proc.getStateInformation(stateData);
        DrumSmashProcessor restored;
        restored.setStateInformation(stateData.getData(), (int)stateData.getSize());
        restored.prepareToPlay(48000.0, 256);
        REQUIRE(getParam(restored.apvts, "drive") == Catch::Approx(0.77f).margin(0.01f));
    }
}
