#include <catch2/catch_all.hpp>
#include "PluginProcessor.h"
#include "TestHelpers.h"

// ── Parameter tests ───────────────────────────────────────────────────────────

TEST_CASE("PitchWobble - parameters", "[pitch_wobble]")
{
    PitchWobbleProcessor proc;

    SECTION("parameter IDs exist")
    {
        REQUIRE(proc.apvts.getParameter("depth")  != nullptr);
        REQUIRE(proc.apvts.getParameter("rate")   != nullptr);
        REQUIRE(proc.apvts.getParameter("smooth") != nullptr);
    }

    SECTION("default values match spec")
    {
        REQUIRE(getParam(proc.apvts, "depth")  == Catch::Approx(8.0f).margin(0.1f));
        REQUIRE(getParam(proc.apvts, "rate")   == Catch::Approx(1.0f).margin(0.01f));
        REQUIRE(getParam(proc.apvts, "smooth") == Catch::Approx(0.7f).margin(0.01f));
    }

    SECTION("parameter ranges")
    {
        auto rangeOf = [&](const juce::String& id) {
            return dynamic_cast<juce::RangedAudioParameter*>(
                proc.apvts.getParameter(id))->getNormalisableRange();
        };
        REQUIRE(rangeOf("depth").start  == Catch::Approx(0.0f));
        REQUIRE(rangeOf("depth").end    == Catch::Approx(30.0f));
        REQUIRE(rangeOf("rate").start   == Catch::Approx(0.1f));
        REQUIRE(rangeOf("rate").end     == Catch::Approx(5.0f));
        REQUIRE(rangeOf("smooth").start == Catch::Approx(0.0f));
        REQUIRE(rangeOf("smooth").end   == Catch::Approx(1.0f));
    }

    SECTION("preset count")
    {
        REQUIRE(proc.getNumPrograms() == PitchWobbleProcessor::NUM_PRESETS);
        REQUIRE(PitchWobbleProcessor::NUM_PRESETS == 8);
    }

    SECTION("preset names")
    {
        REQUIRE(proc.getProgramName(0) == "Breath of Life");
        REQUIRE(proc.getProgramName(7) == "Total Dissolution");
    }

    SECTION("tail length is zero")
    {
        REQUIRE(proc.getTailLengthSeconds() == Catch::Approx(0.0));
    }
}

// ── Preset tests ──────────────────────────────────────────────────────────────

TEST_CASE("PitchWobble - presets", "[pitch_wobble]")
{
    PitchWobbleProcessor proc;

    for (int i = 0; i < PitchWobbleProcessor::NUM_PRESETS; ++i)
    {
        proc.setCurrentProgram(i);
        REQUIRE(proc.getCurrentProgram() == i);
        REQUIRE(getParam(proc.apvts, "depth")  ==
            Catch::Approx(PitchWobbleProcessor::presets[i].depth).margin(0.1f));
        REQUIRE(getParam(proc.apvts, "rate")   ==
            Catch::Approx(PitchWobbleProcessor::presets[i].rate).margin(0.01f));
        REQUIRE(getParam(proc.apvts, "smooth") ==
            Catch::Approx(PitchWobbleProcessor::presets[i].smoothness).margin(0.01f));
    }
}

// ── Audio processing tests ────────────────────────────────────────────────────

TEST_CASE("PitchWobble - audio processing", "[pitch_wobble]")
{
    PitchWobbleProcessor proc;
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
        fillWithSine(buf, 440.f, 44100.0);
        proc.processBlock(buf, midi);
        REQUIRE(allFinite(buf));
        REQUIRE(allBounded(buf, 2.f));
    }

    SECTION("depth=0 produces output close to input level")
    {
        setParam(proc.apvts, "depth", 0.0f);

        // Prime the circular buffer and let smoothed parameters converge
        for (int i = 0; i < 50; ++i)
        {
            juce::AudioBuffer<float> warm(2, 512);
            fillWithSine(warm, 440.f, 44100.0);
            proc.processBlock(warm, midi);
        }

        juce::AudioBuffer<float> buf(2, 512);
        fillWithSine(buf, 440.f, 44100.0);
        float inputRms = rmsOf(buf);
        proc.processBlock(buf, midi);

        REQUIRE(allFinite(buf));
        // With depth=0 the output level should be close to input level
        REQUIRE(rmsOf(buf) > inputRms * 0.3f);
    }

    SECTION("depth=max produces non-zero output from signal")
    {
        setParam(proc.apvts, "depth", 30.f);
        setParam(proc.apvts, "rate",   5.f);
        setParam(proc.apvts, "smooth", 0.1f);

        // Prime so the effect is in steady state
        for (int i = 0; i < 50; ++i)
        {
            juce::AudioBuffer<float> warm(2, 512);
            fillWithSine(warm, 440.f, 44100.0, 0.7f);
            proc.processBlock(warm, midi);
        }

        juce::AudioBuffer<float> buf(2, 512);
        fillWithSine(buf, 440.f, 44100.0, 0.7f);
        proc.processBlock(buf, midi);
        REQUIRE(allFinite(buf));
        REQUIRE(rmsOf(buf) > 0.01f);
    }

    SECTION("extreme parameters do not crash or produce NaN")
    {
        for (auto depth : { 0.f, 30.f })
        for (auto rate  : { 0.1f, 5.f })
        for (auto sm    : { 0.f, 1.f })
        {
            setParam(proc.apvts, "depth",  depth);
            setParam(proc.apvts, "rate",   rate);
            setParam(proc.apvts, "smooth", sm);
            juce::AudioBuffer<float> buf(2, 512);
            fillWithSine(buf, 440.f, 44100.0);
            proc.processBlock(buf, midi);
            REQUIRE(allFinite(buf));
        }
    }

    SECTION("process at 48kHz sample rate")
    {
        PitchWobbleProcessor proc48;
        proc48.setPlayConfigDetails(2, 2, 48000.0, 512);
        proc48.prepareToPlay(48000.0, 512);
        juce::AudioBuffer<float> buf(2, 512);
        fillWithSine(buf, 440.f, 48000.0);
        proc48.processBlock(buf, midi);
        REQUIRE(allFinite(buf));
    }
}

// ── State management tests ────────────────────────────────────────────────────

TEST_CASE("PitchWobble - state management", "[pitch_wobble]")
{
    SECTION("getStateInformation / setStateInformation round-trip")
    {
        PitchWobbleProcessor proc;
        setParam(proc.apvts, "depth",  15.f);
        setParam(proc.apvts, "rate",    2.5f);
        setParam(proc.apvts, "smooth",  0.5f);

        juce::MemoryBlock stateData;
        proc.getStateInformation(stateData);
        REQUIRE(stateData.getSize() > 0);

        setParam(proc.apvts, "depth",  0.f);
        proc.setStateInformation(stateData.getData(), (int)stateData.getSize());

        REQUIRE(getParam(proc.apvts, "depth")  == Catch::Approx(15.f).margin(0.1f));
        REQUIRE(getParam(proc.apvts, "rate")   == Catch::Approx(2.5f).margin(0.05f));
        REQUIRE(getParam(proc.apvts, "smooth") == Catch::Approx(0.5f).margin(0.01f));
    }
}
