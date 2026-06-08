#include <catch2/catch_all.hpp>
#include "PluginProcessor.h"
#include "TestHelpers.h"

// ── Parameter tests ───────────────────────────────────────────────────────────

TEST_CASE("ThroughTheWall - parameters", "[through_the_wall]")
{
    ThroughTheWallAudioProcessor proc;

    SECTION("parameter IDs exist")
    {
        REQUIRE(proc.apvts.getParameter("thickness") != nullptr);
        REQUIRE(proc.apvts.getParameter("bleed")     != nullptr);
        REQUIRE(proc.apvts.getParameter("rattle")    != nullptr);
        REQUIRE(proc.apvts.getParameter("distance")  != nullptr);
    }

    SECTION("default values match spec")
    {
        REQUIRE(getParam(proc.apvts, "thickness") == Catch::Approx(0.5f).margin(0.001f));
        REQUIRE(getParam(proc.apvts, "bleed")     == Catch::Approx(0.35f).margin(0.001f));
        REQUIRE(getParam(proc.apvts, "rattle")    == Catch::Approx(0.2f).margin(0.001f));
        REQUIRE(getParam(proc.apvts, "distance")  == Catch::Approx(0.5f).margin(0.001f));
    }

    SECTION("all parameters are in [0, 1] range")
    {
        for (const char* id : { "thickness", "bleed", "rattle", "distance" })
        {
            auto* p = dynamic_cast<juce::RangedAudioParameter*>(proc.apvts.getParameter(id));
            REQUIRE(p->getNormalisableRange().start == Catch::Approx(0.f));
            REQUIRE(p->getNormalisableRange().end   == Catch::Approx(1.f));
        }
    }

    SECTION("preset count")
    {
        REQUIRE(proc.getNumPrograms() == kNumPresets);
        REQUIRE(kNumPresets == 6);
    }

    SECTION("preset names")
    {
        REQUIRE(proc.getProgramName(0) == "Thin Partition");
        REQUIRE(proc.getProgramName(1) == "Apartment Next Door");
        REQUIRE(proc.getProgramName(5) == "Ghost Room");
    }
}

// ── Preset tests ──────────────────────────────────────────────────────────────

TEST_CASE("ThroughTheWall - presets", "[through_the_wall]")
{
    ThroughTheWallAudioProcessor proc;

    for (int i = 0; i < kNumPresets; ++i)
    {
        proc.applyPreset(i);
        REQUIRE(getParam(proc.apvts, "thickness") == Catch::Approx(kPresets[i].thickness).margin(0.001f));
        REQUIRE(getParam(proc.apvts, "bleed")     == Catch::Approx(kPresets[i].bleed).margin(0.001f));
        REQUIRE(getParam(proc.apvts, "rattle")    == Catch::Approx(kPresets[i].rattle).margin(0.001f));
        REQUIRE(getParam(proc.apvts, "distance")  == Catch::Approx(kPresets[i].distance).margin(0.001f));
    }
}

// ── Audio processing tests ────────────────────────────────────────────────────

TEST_CASE("ThroughTheWall - audio processing", "[through_the_wall]")
{
    ThroughTheWallAudioProcessor proc;
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

    SECTION("high thickness attenuates high-frequency content more than low thickness")
    {
        // Low thickness: mostly passes 1kHz
        setParam(proc.apvts, "thickness", 0.0f);
        setParam(proc.apvts, "bleed",     0.0f);
        setParam(proc.apvts, "rattle",    0.0f);
        setParam(proc.apvts, "distance",  0.0f);
        proc.prepareToPlay(44100.0, 512);
        flushWithSilence(proc);

        juce::AudioBuffer<float> buf(2, 512);
        fillWithSine(buf, 1000.f, 44100.0, 0.5f);
        proc.processBlock(buf, midi);
        float rmsLowThick = rmsOf(buf);

        // High thickness: heavily filters 1kHz
        setParam(proc.apvts, "thickness", 1.0f);
        proc.prepareToPlay(44100.0, 512);
        flushWithSilence(proc);

        fillWithSine(buf, 1000.f, 44100.0, 0.5f);
        proc.processBlock(buf, midi);
        float rmsHighThick = rmsOf(buf);

        REQUIRE(rmsHighThick < rmsLowThick * 0.8f);
    }

    SECTION("high distance attenuates output more than low distance")
    {
        setParam(proc.apvts, "thickness", 0.5f);
        setParam(proc.apvts, "bleed",     0.0f);
        setParam(proc.apvts, "rattle",    0.0f);

        setParam(proc.apvts, "distance", 0.0f);
        proc.prepareToPlay(44100.0, 512);
        flushWithSilence(proc);

        juce::AudioBuffer<float> buf(2, 512);
        fillWithSine(buf, 440.f, 44100.0, 0.5f);
        proc.processBlock(buf, midi);
        float rmsNear = rmsOf(buf);

        setParam(proc.apvts, "distance", 1.0f);
        proc.prepareToPlay(44100.0, 512);
        flushWithSilence(proc);

        fillWithSine(buf, 440.f, 44100.0, 0.5f);
        proc.processBlock(buf, midi);
        float rmsFar = rmsOf(buf);

        REQUIRE(rmsFar < rmsNear * 0.8f);
    }

    SECTION("all extreme parameter combinations do not crash")
    {
        for (auto thickness : { 0.f, 1.f })
        for (auto bleed     : { 0.f, 1.f })
        for (auto rattle    : { 0.f, 1.f })
        for (auto distance  : { 0.f, 1.f })
        {
            setParam(proc.apvts, "thickness", thickness);
            setParam(proc.apvts, "bleed",     bleed);
            setParam(proc.apvts, "rattle",    rattle);
            setParam(proc.apvts, "distance",  distance);
            proc.prepareToPlay(44100.0, 512);
            juce::AudioBuffer<float> buf(2, 512);
            fillWithSine(buf, 440.f, 44100.0);
            proc.processBlock(buf, midi);
            REQUIRE(allFinite(buf));
        }
    }
}

// ── State management tests ────────────────────────────────────────────────────

TEST_CASE("ThroughTheWall - state management", "[through_the_wall]")
{
    SECTION("getStateInformation / setStateInformation round-trip")
    {
        ThroughTheWallAudioProcessor proc;
        setParam(proc.apvts, "thickness", 0.7f);
        setParam(proc.apvts, "bleed",     0.6f);
        setParam(proc.apvts, "rattle",    0.4f);
        setParam(proc.apvts, "distance",  0.8f);

        juce::MemoryBlock stateData;
        proc.getStateInformation(stateData);
        REQUIRE(stateData.getSize() > 0);

        setParam(proc.apvts, "thickness", 0.1f);
        proc.setStateInformation(stateData.getData(), (int)stateData.getSize());

        REQUIRE(getParam(proc.apvts, "thickness") == Catch::Approx(0.7f).margin(0.001f));
        REQUIRE(getParam(proc.apvts, "bleed")     == Catch::Approx(0.6f).margin(0.001f));
        REQUIRE(getParam(proc.apvts, "rattle")    == Catch::Approx(0.4f).margin(0.001f));
        REQUIRE(getParam(proc.apvts, "distance")  == Catch::Approx(0.8f).margin(0.001f));
    }

    SECTION("prepareToPlay then releaseResources does not crash")
    {
        ThroughTheWallAudioProcessor proc;
        proc.prepareToPlay(44100.0, 512);
        proc.releaseResources();
        proc.prepareToPlay(48000.0, 256);
        proc.releaseResources();
    }
}
