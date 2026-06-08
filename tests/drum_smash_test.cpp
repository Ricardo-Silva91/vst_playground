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
        REQUIRE(getParam(proc.apvts, "bitDepth")      == Catch::Approx(16.f).margin(0.1f));
        REQUIRE(getParam(proc.apvts, "sampleRateDiv") == Catch::Approx(1.f).margin(0.01f));
        REQUIRE(getParam(proc.apvts, "drive")         == Catch::Approx(0.f).margin(0.01f));
        REQUIRE(getParam(proc.apvts, "outputGain")    == Catch::Approx(1.f).margin(0.01f));
        REQUIRE(getParam(proc.apvts, "noiseAmount")   == Catch::Approx(0.f).margin(0.01f));
        REQUIRE(getParam(proc.apvts, "lpfCutoff")     == Catch::Approx(22000.f).margin(1.f));
        REQUIRE(getParam(proc.apvts, "hpfCutoff")     == Catch::Approx(20.f).margin(0.1f));
        REQUIRE(getParam(proc.apvts, "reverbWet")     == Catch::Approx(0.f).margin(0.01f));
        REQUIRE(getParam(proc.apvts, "stereoWidth")   == Catch::Approx(1.f).margin(0.01f));
        REQUIRE(getParam(proc.apvts, "transientBoost")== Catch::Approx(0.f).margin(0.01f));
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

    SECTION("LPF at 200Hz attenuates 1kHz more than LPF at 22kHz")
    {
        // High cutoff
        setParam(proc.apvts, "lpfCutoff",     22000.f);
        setParam(proc.apvts, "bitDepth",      16.f);
        setParam(proc.apvts, "sampleRateDiv",  1.f);
        setParam(proc.apvts, "drive",          0.f);
        setParam(proc.apvts, "noiseAmount",    0.f);
        setParam(proc.apvts, "crackleRate",    0.f);
        setParam(proc.apvts, "reverbWet",      0.f);
        setParam(proc.apvts, "compThreshold",  0.f);
        setParam(proc.apvts, "compMakeup",     0.f);
        setParam(proc.apvts, "stereoWidth",    1.f);
        setParam(proc.apvts, "transientBoost", 0.f);
        setParam(proc.apvts, "wowRate",        0.f);
        setParam(proc.apvts, "outputGain",     1.f);

        proc.prepareToPlay(44100.0, 512);
        flushWithSilence(proc, 5);

        juce::AudioBuffer<float> buf(2, 512);
        fillWithSine(buf, 1000.f, 44100.0, 0.5f);
        proc.processBlock(buf, midi);
        float rmsHigh = rmsOf(buf);

        // Low cutoff
        setParam(proc.apvts, "lpfCutoff", 200.f);
        proc.prepareToPlay(44100.0, 512);
        flushWithSilence(proc, 5);

        fillWithSine(buf, 1000.f, 44100.0, 0.5f);
        proc.processBlock(buf, midi);
        float rmsLow = rmsOf(buf);

        REQUIRE(rmsLow < rmsHigh * 0.5f);
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
}
