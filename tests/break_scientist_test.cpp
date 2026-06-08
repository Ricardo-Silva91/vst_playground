#include <catch2/catch_all.hpp>
#include "PluginProcessor.h"
#include "TestHelpers.h"

// ── Parameter tests ───────────────────────────────────────────────────────────

TEST_CASE("BreakScientist - parameters", "[break_scientist]")
{
    BreakScientistProcessor proc;

    SECTION("parameter IDs exist")
    {
        REQUIRE(proc.apvts.getParameter("swing")       != nullptr);
        REQUIRE(proc.apvts.getParameter("humanize")    != nullptr);
        REQUIRE(proc.apvts.getParameter("drag")        != nullptr);
        REQUIRE(proc.apvts.getParameter("sensitivity") != nullptr);
        REQUIRE(proc.apvts.getParameter("velocityvar") != nullptr);
        REQUIRE(proc.apvts.getParameter("wetmix")      != nullptr);
    }

    SECTION("default values match spec")
    {
        REQUIRE(getParam(proc.apvts, "swing")       == Catch::Approx(0.58f).margin(0.001f));
        REQUIRE(getParam(proc.apvts, "humanize")    == Catch::Approx(0.2f).margin(0.001f));
        REQUIRE(getParam(proc.apvts, "drag")        == Catch::Approx(0.1f).margin(0.001f));
        REQUIRE(getParam(proc.apvts, "sensitivity") == Catch::Approx(0.5f).margin(0.001f));
        REQUIRE(getParam(proc.apvts, "velocityvar") == Catch::Approx(0.3f).margin(0.001f));
        REQUIRE(getParam(proc.apvts, "wetmix")      == Catch::Approx(1.0f).margin(0.001f));
    }

    SECTION("parameter ranges")
    {
        auto rangeOf = [&](const juce::String& id) {
            return dynamic_cast<juce::RangedAudioParameter*>(
                proc.apvts.getParameter(id))->getNormalisableRange();
        };
        // swing has a non-standard range starting at 0.5
        REQUIRE(rangeOf("swing").start   == Catch::Approx(0.50f));
        REQUIRE(rangeOf("swing").end     == Catch::Approx(0.85f));
        REQUIRE(rangeOf("humanize").start== Catch::Approx(0.f));
        REQUIRE(rangeOf("humanize").end  == Catch::Approx(1.f));
        REQUIRE(rangeOf("wetmix").start  == Catch::Approx(0.f));
        REQUIRE(rangeOf("wetmix").end    == Catch::Approx(1.f));
    }

    SECTION("preset count")
    {
        REQUIRE(proc.getNumPrograms() == kNumPresets);
        REQUIRE(kNumPresets == 10);
    }

    SECTION("preset names")
    {
        REQUIRE(proc.getProgramName(0) == "Donuts");
        REQUIRE(proc.getProgramName(1) == "MPC Straight");
        REQUIRE(proc.getProgramName(9) == "Trap Drunk");
    }

    SECTION("tail length")
    {
        REQUIRE(proc.getTailLengthSeconds() == Catch::Approx(2.0));
    }
}

// ── Preset tests ──────────────────────────────────────────────────────────────

TEST_CASE("BreakScientist - presets", "[break_scientist]")
{
    BreakScientistProcessor proc;
    proc.setPlayConfigDetails(2, 2, 44100.0, 512);
    proc.prepareToPlay(44100.0, 512);

    for (int i = 0; i < kNumPresets; ++i)
    {
        proc.setCurrentProgram(i);
        REQUIRE(proc.getCurrentProgram() == i);
        REQUIRE(getParam(proc.apvts, "swing")    == Catch::Approx(kPresets[i].swing).margin(0.001f));
        REQUIRE(getParam(proc.apvts, "humanize") == Catch::Approx(kPresets[i].humanize).margin(0.001f));
        REQUIRE(getParam(proc.apvts, "drag")     == Catch::Approx(kPresets[i].drag).margin(0.001f));
        REQUIRE(getParam(proc.apvts, "wetmix")   == Catch::Approx(kPresets[i].wetMix).margin(0.001f));
    }
}

// ── Audio processing tests ────────────────────────────────────────────────────

TEST_CASE("BreakScientist - audio processing", "[break_scientist]")
{
    // prepareToPlay allocates a 2-second ring buffer; use a moderate block size
    BreakScientistProcessor proc;
    proc.setPlayConfigDetails(2, 2, 44100.0, 512);
    proc.prepareToPlay(44100.0, 512);
    juce::MidiBuffer midi;

    SECTION("silence in produces silence out")
    {
        // The ring buffer is zero-initialised; silence should stay silent
        auto buf = makeSilent(2, 512);
        proc.processBlock(buf, midi);
        REQUIRE(allFinite(buf));
        REQUIRE(rmsOf(buf) < 1e-6f);
    }

    SECTION("output is finite and bounded after processing signal")
    {
        juce::AudioBuffer<float> buf(2, 512);
        fillWithSine(buf, 440.f, 44100.0, 0.5f);
        proc.processBlock(buf, midi);
        REQUIRE(allFinite(buf));
        REQUIRE(allBounded(buf, 2.f));
    }

    SECTION("reported latency matches 2-second lookahead at 44100")
    {
        // 44100 * 2 = 88200 samples of latency
        REQUIRE(proc.getLatencySamples() == 88200);
    }

    SECTION("output after 2-second drain contains the original signal")
    {
        // Feed a burst, then drain the 2-second buffer so the burst comes out
        setParam(proc.apvts, "wetmix", 1.0f);
        setParam(proc.apvts, "drag",   0.0f);

        juce::AudioBuffer<float> burst(2, 512);
        fillWithSine(burst, 440.f, 44100.0, 0.5f);
        float burstRms = rmsOf(burst);

        proc.processBlock(burst, midi);

        // Drain 2 seconds worth of blocks
        float maxOutputRms = 0.f;
        for (int n = 0; n < 175; ++n)  // ~175 * 512 ≈ 89600 > 88200
        {
            juce::AudioBuffer<float> drain(2, 512);
            drain.clear();
            proc.processBlock(drain, midi);
            maxOutputRms = std::max(maxOutputRms, rmsOf(drain));
        }

        REQUIRE(allFinite(burst));
        REQUIRE(maxOutputRms > burstRms * 0.01f);
    }

    SECTION("all extreme parameter combinations do not crash")
    {
        for (auto swing : { 0.50f, 0.85f })
        for (auto hum   : { 0.0f,  1.0f  })
        for (auto wet   : { 0.0f,  1.0f  })
        {
            setParam(proc.apvts, "swing",    swing);
            setParam(proc.apvts, "humanize", hum);
            setParam(proc.apvts, "wetmix",   wet);
            juce::AudioBuffer<float> buf(2, 512);
            fillWithSine(buf, 440.f, 44100.0);
            proc.processBlock(buf, midi);
            REQUIRE(allFinite(buf));
        }
    }
}

// ── State management tests ────────────────────────────────────────────────────

TEST_CASE("BreakScientist - state management", "[break_scientist]")
{
    SECTION("getStateInformation / setStateInformation round-trip")
    {
        BreakScientistProcessor proc;
        setParam(proc.apvts, "swing",    0.75f);
        setParam(proc.apvts, "humanize", 0.8f);
        setParam(proc.apvts, "drag",     0.5f);
        setParam(proc.apvts, "wetmix",   0.6f);

        juce::MemoryBlock stateData;
        proc.getStateInformation(stateData);
        REQUIRE(stateData.getSize() > 0);

        setParam(proc.apvts, "swing", 0.50f);
        proc.setStateInformation(stateData.getData(), (int)stateData.getSize());

        REQUIRE(getParam(proc.apvts, "swing")    == Catch::Approx(0.75f).margin(0.001f));
        REQUIRE(getParam(proc.apvts, "humanize") == Catch::Approx(0.8f).margin(0.001f));
        REQUIRE(getParam(proc.apvts, "drag")     == Catch::Approx(0.5f).margin(0.001f));
        REQUIRE(getParam(proc.apvts, "wetmix")   == Catch::Approx(0.6f).margin(0.001f));
    }

    SECTION("prepareToPlay can be called multiple times")
    {
        BreakScientistProcessor proc;
        proc.prepareToPlay(44100.0, 512);
        proc.releaseResources();
        proc.prepareToPlay(48000.0, 256);
        proc.releaseResources();
    }
}
