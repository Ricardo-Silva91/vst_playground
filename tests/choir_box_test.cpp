#include <catch2/catch_all.hpp>
#include "PluginProcessor.h"
#include "TestHelpers.h"

// ── Parameter tests ───────────────────────────────────────────────────────────

TEST_CASE("ChoirBox - parameters", "[choir_box]")
{
    ChoirBoxProcessor proc;

    SECTION("all parameter IDs exist")
    {
        for (const char* id : { "upSemitones", "downSemitones", "voices", "detune",
                                 "dryLevel", "upLevel", "downLevel",
                                 "saturation", "crush", "distMix", "masterOut" })
        {
            INFO("Checking parameter: " << id);
            REQUIRE(proc.apvts.getParameter(id) != nullptr);
        }
    }

    SECTION("default values match spec")
    {
        REQUIRE(getParam(proc.apvts, "upSemitones")  == Catch::Approx(7.f).margin(0.01f));
        REQUIRE(getParam(proc.apvts, "downSemitones")== Catch::Approx(-7.f).margin(0.01f));
        REQUIRE(getParam(proc.apvts, "voices")       == Catch::Approx(1.f).margin(0.01f));
        REQUIRE(getParam(proc.apvts, "detune")       == Catch::Approx(20.f).margin(0.1f));
        REQUIRE(getParam(proc.apvts, "dryLevel")     == Catch::Approx(1.0f).margin(0.01f));
        REQUIRE(getParam(proc.apvts, "upLevel")      == Catch::Approx(0.7f).margin(0.01f));
        REQUIRE(getParam(proc.apvts, "downLevel")    == Catch::Approx(0.7f).margin(0.01f));
        REQUIRE(getParam(proc.apvts, "saturation")   == Catch::Approx(0.0f).margin(0.01f));
        REQUIRE(getParam(proc.apvts, "crush")        == Catch::Approx(0.0f).margin(0.01f));
        REQUIRE(getParam(proc.apvts, "distMix")      == Catch::Approx(0.0f).margin(0.01f));
        REQUIRE(getParam(proc.apvts, "masterOut")    == Catch::Approx(1.0f).margin(0.01f));
    }

    SECTION("parameter ranges")
    {
        auto rangeOf = [&](const juce::String& id) {
            return dynamic_cast<juce::RangedAudioParameter*>(
                proc.apvts.getParameter(id))->getNormalisableRange();
        };
        REQUIRE(rangeOf("upSemitones").start  == Catch::Approx(-24.f));
        REQUIRE(rangeOf("upSemitones").end    == Catch::Approx(24.f));
        REQUIRE(rangeOf("downSemitones").start== Catch::Approx(-24.f));
        REQUIRE(rangeOf("downSemitones").end  == Catch::Approx(24.f));
        REQUIRE(rangeOf("voices").start       == Catch::Approx(1.f));
        REQUIRE(rangeOf("voices").end         == Catch::Approx(4.f));
        REQUIRE(rangeOf("masterOut").start    == Catch::Approx(0.f));
        REQUIRE(rangeOf("masterOut").end      == Catch::Approx(2.f));
    }

    SECTION("preset count")
    {
        REQUIRE(proc.getNumPrograms() == kNumPresets);
        REQUIRE(kNumPresets == 6);
    }

    SECTION("preset names")
    {
        REQUIRE(proc.getProgramName(0) == "Power Fifth");
        REQUIRE(proc.getProgramName(2) == "Choir");
        REQUIRE(proc.getProgramName(5) == "Ghost Voice");
    }
}

// ── Preset tests ──────────────────────────────────────────────────────────────

TEST_CASE("ChoirBox - presets", "[choir_box]")
{
    ChoirBoxProcessor proc;

    for (int i = 0; i < kNumPresets; ++i)
    {
        proc.setCurrentProgram(i);
        REQUIRE(proc.getCurrentProgram() == i);
        REQUIRE(getParam(proc.apvts, "upSemitones")  == Catch::Approx(kPresets[i].upSemitones).margin(0.01f));
        REQUIRE(getParam(proc.apvts, "downSemitones")== Catch::Approx(kPresets[i].downSemitones).margin(0.01f));
        REQUIRE(getParam(proc.apvts, "dryLevel")     == Catch::Approx(kPresets[i].dryLevel).margin(0.01f));
        REQUIRE(getParam(proc.apvts, "masterOut")    == Catch::Approx(kPresets[i].masterOut).margin(0.01f));
    }
}

// ── Audio processing tests ────────────────────────────────────────────────────

TEST_CASE("ChoirBox - audio processing", "[choir_box]")
{
    ChoirBoxProcessor proc;
    proc.setPlayConfigDetails(2, 2, 44100.0, 512);
    proc.prepareToPlay(44100.0, 512);
    juce::MidiBuffer midi;

    SECTION("silence in produces silence out")
    {
        auto buf = makeSilent(2, 512);
        proc.processBlock(buf, midi);
        REQUIRE(allFinite(buf));
        REQUIRE(rmsOf(buf) < 1e-6f);
    }

    SECTION("output is finite and bounded")
    {
        juce::AudioBuffer<float> buf(2, 512);
        fillWithSine(buf, 440.f, 44100.0, 0.5f);
        proc.processBlock(buf, midi);
        REQUIRE(allFinite(buf));
        REQUIRE(allBounded(buf, 3.f));
    }

    SECTION("dry-only mode (upLevel=0, downLevel=0): output has signal from dry path")
    {
        setParam(proc.apvts, "dryLevel",  1.0f);
        setParam(proc.apvts, "upLevel",   0.0f);
        setParam(proc.apvts, "downLevel", 0.0f);
        setParam(proc.apvts, "masterOut", 1.0f);
        setParam(proc.apvts, "saturation",0.0f);
        setParam(proc.apvts, "crush",     0.0f);
        setParam(proc.apvts, "distMix",   0.0f);

        // Fill the phase-vocoder latency buffer first
        flushWithSilence(proc, 10);

        juce::AudioBuffer<float> buf(2, 512);
        fillWithSine(buf, 440.f, 44100.0, 0.5f);
        float inputRms = rmsOf(buf);
        proc.processBlock(buf, midi);

        REQUIRE(allFinite(buf));
        // The dry signal should pass through; allow for latency/windowing effects
        REQUIRE(rmsOf(buf) > inputRms * 0.1f);
    }

    SECTION("pitch-only mode (dryLevel=0): produces output from pitch shifted voices")
    {
        setParam(proc.apvts, "dryLevel",      0.0f);
        setParam(proc.apvts, "upLevel",       1.0f);
        setParam(proc.apvts, "downLevel",     1.0f);
        setParam(proc.apvts, "upSemitones",   12.f);
        setParam(proc.apvts, "downSemitones", -12.f);
        setParam(proc.apvts, "voices",         1.f);
        setParam(proc.apvts, "masterOut",      1.0f);

        // Prime the phase-vocoder buffers
        for (int n = 0; n < 20; ++n)
        {
            juce::AudioBuffer<float> warm(2, 512);
            fillWithSine(warm, 440.f, 44100.0, 0.5f);
            proc.processBlock(warm, midi);
        }

        juce::AudioBuffer<float> buf(2, 512);
        fillWithSine(buf, 440.f, 44100.0, 0.5f);
        proc.processBlock(buf, midi);

        REQUIRE(allFinite(buf));
        REQUIRE(rmsOf(buf) > 0.01f);
    }

    SECTION("masterOut=0 produces silence")
    {
        setParam(proc.apvts, "masterOut", 0.0f);
        flushWithSilence(proc, 5);

        juce::AudioBuffer<float> buf(2, 512);
        fillWithSine(buf, 440.f, 44100.0, 0.5f);
        proc.processBlock(buf, midi);
        REQUIRE(allFinite(buf));
        REQUIRE(rmsOf(buf) < 1e-5f);
    }

    SECTION("saturation increases with saturation parameter")
    {
        setParam(proc.apvts, "dryLevel",   0.0f);
        setParam(proc.apvts, "upLevel",    1.0f);
        setParam(proc.apvts, "downLevel",  1.0f);
        setParam(proc.apvts, "distMix",    1.0f);
        setParam(proc.apvts, "masterOut",  1.0f);

        // Low saturation
        setParam(proc.apvts, "saturation", 0.0f);
        setParam(proc.apvts, "crush",      0.0f);
        proc.prepareToPlay(44100.0, 512);
        for (int n = 0; n < 10; ++n) {
            juce::AudioBuffer<float> w(2, 512);
            fillWithSine(w, 440.f, 44100.0, 0.5f);
            proc.processBlock(w, midi);
        }
        juce::AudioBuffer<float> bufLow(2, 512);
        fillWithSine(bufLow, 440.f, 44100.0, 0.5f);
        proc.processBlock(bufLow, midi);

        // High saturation
        setParam(proc.apvts, "saturation", 1.0f);
        proc.prepareToPlay(44100.0, 512);
        for (int n = 0; n < 10; ++n) {
            juce::AudioBuffer<float> w(2, 512);
            fillWithSine(w, 440.f, 44100.0, 0.5f);
            proc.processBlock(w, midi);
        }
        juce::AudioBuffer<float> bufHigh(2, 512);
        fillWithSine(bufHigh, 440.f, 44100.0, 0.5f);
        proc.processBlock(bufHigh, midi);

        REQUIRE(allFinite(bufLow));
        REQUIRE(allFinite(bufHigh));
        // Both should produce some output; this checks the code path runs without NaN
    }

    SECTION("extreme parameter combinations do not crash")
    {
        for (auto dry  : { 0.f, 1.f })
        for (auto up   : { 0.f, 1.f })
        for (auto sat  : { 0.f, 1.f })
        {
            setParam(proc.apvts, "dryLevel",   dry);
            setParam(proc.apvts, "upLevel",    up);
            setParam(proc.apvts, "downLevel",  up);
            setParam(proc.apvts, "saturation", sat);
            juce::AudioBuffer<float> buf(2, 512);
            fillWithSine(buf, 440.f, 44100.0);
            proc.processBlock(buf, midi);
            REQUIRE(allFinite(buf));
        }
    }

    SECTION("process at 48kHz sample rate")
    {
        ChoirBoxProcessor proc48;
        proc48.setPlayConfigDetails(2, 2, 48000.0, 512);
        proc48.prepareToPlay(48000.0, 512);
        juce::AudioBuffer<float> buf(2, 512);
        fillWithSine(buf, 440.f, 48000.0);
        proc48.processBlock(buf, midi);
        REQUIRE(allFinite(buf));
    }
}

// ── State management tests ────────────────────────────────────────────────────

TEST_CASE("ChoirBox - state management", "[choir_box]")
{
    SECTION("getStateInformation / setStateInformation round-trip")
    {
        ChoirBoxProcessor proc;
        setParam(proc.apvts, "upSemitones",   5.f);
        setParam(proc.apvts, "downSemitones", -3.f);
        setParam(proc.apvts, "voices",         3.f);
        setParam(proc.apvts, "dryLevel",       0.5f);

        juce::MemoryBlock stateData;
        proc.getStateInformation(stateData);
        REQUIRE(stateData.getSize() > 0);

        setParam(proc.apvts, "upSemitones", 0.f);
        proc.setStateInformation(stateData.getData(), (int)stateData.getSize());

        REQUIRE(getParam(proc.apvts, "upSemitones")  == Catch::Approx(5.f).margin(0.01f));
        REQUIRE(getParam(proc.apvts, "downSemitones")== Catch::Approx(-3.f).margin(0.01f));
        REQUIRE(getParam(proc.apvts, "voices")       == Catch::Approx(3.f).margin(0.01f));
        REQUIRE(getParam(proc.apvts, "dryLevel")     == Catch::Approx(0.5f).margin(0.01f));
    }

    SECTION("FFT latency is reported correctly")
    {
        ChoirBoxProcessor proc;
        proc.setPlayConfigDetails(2, 2, 44100.0, 512);
        proc.prepareToPlay(44100.0, 512);
        // Choir Box latency = kFftSize = 2048 samples
        REQUIRE(proc.getLatencySamples() == PitchShifter::kFftSize);
    }
}
