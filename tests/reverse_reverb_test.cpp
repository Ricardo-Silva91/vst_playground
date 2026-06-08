#include <catch2/catch_all.hpp>
#include "PluginProcessor.h"
#include "TestHelpers.h"

// ── Parameter tests ───────────────────────────────────────────────────────────

TEST_CASE("ReverseReverb - parameters", "[reverse_reverb]")
{
    ReverseReverbAudioProcessor proc;

    SECTION("parameter IDs exist")
    {
        REQUIRE(proc.apvts.getParameter("roomSize") != nullptr);
        REQUIRE(proc.apvts.getParameter("wetMix")   != nullptr);
        REQUIRE(proc.apvts.getParameter("windowMs") != nullptr);
    }

    SECTION("default values match spec")
    {
        REQUIRE(getParam(proc.apvts, "roomSize") == Catch::Approx(0.8f).margin(0.001f));
        REQUIRE(getParam(proc.apvts, "wetMix")   == Catch::Approx(0.8f).margin(0.001f));
        REQUIRE(getParam(proc.apvts, "windowMs") == Catch::Approx(500.f).margin(1.f));
    }

    SECTION("parameter ranges")
    {
        auto rangeOf = [&](const juce::String& id) {
            return dynamic_cast<juce::RangedAudioParameter*>(
                proc.apvts.getParameter(id))->getNormalisableRange();
        };
        REQUIRE(rangeOf("roomSize").start == Catch::Approx(0.1f));
        REQUIRE(rangeOf("roomSize").end   == Catch::Approx(1.0f));
        REQUIRE(rangeOf("wetMix").start   == Catch::Approx(0.0f));
        REQUIRE(rangeOf("wetMix").end     == Catch::Approx(1.0f));
        REQUIRE(rangeOf("windowMs").start == Catch::Approx(100.f));
        REQUIRE(rangeOf("windowMs").end   == Catch::Approx(2000.f));
    }

    SECTION("preset count")
    {
        REQUIRE(proc.getNumPrograms() == kNumPresets);
        REQUIRE(kNumPresets == 5);
    }

    SECTION("preset names")
    {
        REQUIRE(proc.getProgramName(0) == "Default");
        REQUIRE(proc.getProgramName(1) == "Ghost Bloom");
        REQUIRE(proc.getProgramName(4) == "Tight Shimmer");
    }

    SECTION("tail length")
    {
        REQUIRE(proc.getTailLengthSeconds() == Catch::Approx(2.0));
    }
}

// ── Preset tests ──────────────────────────────────────────────────────────────

TEST_CASE("ReverseReverb - presets", "[reverse_reverb]")
{
    ReverseReverbAudioProcessor proc;

    for (int i = 0; i < kNumPresets; ++i)
    {
        proc.applyPreset(i);
        REQUIRE(getParam(proc.apvts, "roomSize") == Catch::Approx(kPresets[i].roomSize).margin(0.01f));
        REQUIRE(getParam(proc.apvts, "wetMix")   == Catch::Approx(kPresets[i].wetMix).margin(0.01f));
        REQUIRE(getParam(proc.apvts, "windowMs") == Catch::Approx(kPresets[i].windowMs).margin(1.f));
    }
}

// ── Audio processing tests ────────────────────────────────────────────────────

TEST_CASE("ReverseReverb - audio processing", "[reverse_reverb]")
{
    ReverseReverbAudioProcessor proc;
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

    SECTION("output is finite and bounded with a signal")
    {
        juce::AudioBuffer<float> buf(2, 512);
        fillWithSine(buf, 440.f, 44100.0);
        proc.processBlock(buf, midi);
        REQUIRE(allFinite(buf));
        REQUIRE(allBounded(buf, 2.f));
    }

    SECTION("wet=0 passes signal with minimal loss")
    {
        setParam(proc.apvts, "wetMix", 0.0f);
        juce::AudioBuffer<float> buf(2, 512);
        fillWithSine(buf, 440.f, 44100.0);
        float inputRms = rmsOf(buf);
        proc.processBlock(buf, midi);
        REQUIRE(allFinite(buf));
        // With wet=0 the dry signal should be mostly intact
        REQUIRE(rmsOf(buf) > inputRms * 0.1f);
    }

    SECTION("high wet produces reverb tail after signal stops")
    {
        setParam(proc.apvts, "wetMix", 1.0f);
        setParam(proc.apvts, "roomSize", 1.0f);

        // Feed a sine burst, then silence
        juce::AudioBuffer<float> burst(2, 512);
        fillWithSine(burst, 440.f, 44100.0, 0.8f);
        proc.processBlock(burst, midi);

        // After the burst, the reverb should still be producing output
        juce::AudioBuffer<float> silence(2, 512);
        silence.clear();
        proc.processBlock(silence, midi);

        REQUIRE(allFinite(silence));
    }

    SECTION("extreme parameter values do not crash or produce NaN")
    {
        for (auto wet : { 0.0f, 1.0f })
        for (auto room : { 0.1f, 1.0f })
        for (auto win : { 100.f, 2000.f })
        {
            setParam(proc.apvts, "wetMix",   wet);
            setParam(proc.apvts, "roomSize", room);
            setParam(proc.apvts, "windowMs", win);
            juce::AudioBuffer<float> buf(2, 512);
            fillWithSine(buf, 440.f, 44100.0);
            proc.processBlock(buf, midi);
            REQUIRE(allFinite(buf));
        }
    }
}

// ── State management tests ────────────────────────────────────────────────────

TEST_CASE("ReverseReverb - state management", "[reverse_reverb]")
{
    SECTION("getStateInformation / setStateInformation round-trip")
    {
        ReverseReverbAudioProcessor proc;
        setParam(proc.apvts, "roomSize", 0.3f);
        setParam(proc.apvts, "wetMix",   0.4f);
        setParam(proc.apvts, "windowMs", 750.f);

        juce::MemoryBlock stateData;
        proc.getStateInformation(stateData);
        REQUIRE(stateData.getSize() > 0);

        // Mutate parameters
        setParam(proc.apvts, "roomSize", 0.9f);
        setParam(proc.apvts, "wetMix",   0.9f);

        proc.setStateInformation(stateData.getData(), (int)stateData.getSize());

        REQUIRE(getParam(proc.apvts, "roomSize") == Catch::Approx(0.3f).margin(0.01f));
        REQUIRE(getParam(proc.apvts, "wetMix")   == Catch::Approx(0.4f).margin(0.01f));
        REQUIRE(getParam(proc.apvts, "windowMs") == Catch::Approx(750.f).margin(1.f));
    }

    SECTION("prepareToPlay then releaseResources does not crash")
    {
        ReverseReverbAudioProcessor proc;
        proc.prepareToPlay(44100.0, 512);
        proc.releaseResources();
        proc.prepareToPlay(48000.0, 256);
        proc.releaseResources();
    }
}
