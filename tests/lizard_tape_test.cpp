#include <catch2/catch_all.hpp>
#include "PluginProcessor.h"
#include "TestHelpers.h"

static const char* const kParamIDs[] = { "drive", "bias", "wowDepth", "wowRate", "flutterDepth",
                                         "flutterRate", "tone", "hiss", "mix", "output" };

// ── Parameter tests ───────────────────────────────────────────────────────────

TEST_CASE("LizardTape - parameters", "[lizard_tape]")
{
    LizardTapeAudioProcessor proc;

    SECTION("all parameter IDs exist")
    {
        for (const char* id : kParamIDs)
        {
            INFO("Checking parameter: " << id);
            REQUIRE(proc.apvts.getParameter(id) != nullptr);
        }
    }

    SECTION("default values")
    {
        REQUIRE(getParam(proc.apvts, "drive")        == Catch::Approx(4.f).margin(0.01f));
        REQUIRE(getParam(proc.apvts, "bias")         == Catch::Approx(0.15f).margin(0.001f));
        REQUIRE(getParam(proc.apvts, "wowDepth")     == Catch::Approx(3.f).margin(0.01f));
        REQUIRE(getParam(proc.apvts, "wowRate")      == Catch::Approx(0.7f).margin(0.001f));
        REQUIRE(getParam(proc.apvts, "flutterDepth") == Catch::Approx(0.6f).margin(0.001f));
        REQUIRE(getParam(proc.apvts, "flutterRate")  == Catch::Approx(8.f).margin(0.01f));
        REQUIRE(getParam(proc.apvts, "tone")         == Catch::Approx(6500.f).margin(1.f));
        REQUIRE(getParam(proc.apvts, "hiss")         == Catch::Approx(0.06f).margin(0.001f));
        REQUIRE(getParam(proc.apvts, "mix")          == Catch::Approx(1.f));
        REQUIRE(getParam(proc.apvts, "output")       == Catch::Approx(0.f).margin(0.01f));
    }
}

// ── Audio processing tests ────────────────────────────────────────────────────

TEST_CASE("LizardTape - audio processing", "[lizard_tape]")
{
    juce::MidiBuffer midi;

    SECTION("output stays finite and bounded at maximum drive")
    {
        LizardTapeAudioProcessor proc;
        setParam(proc.apvts, "drive", 24.f);
        setParam(proc.apvts, "hiss",  1.f);
        proc.prepareToPlay(44100.0, 512);
        for (int n = 0; n < 50; ++n)
        {
            juce::AudioBuffer<float> buf(2, 512);
            fillWithSine(buf, 220.f, 44100.0, 0.9f);
            proc.processBlock(buf, midi);
            REQUIRE(allFinite(buf));
            REQUIRE(allBounded(buf, 4.f));
        }
    }

    SECTION("mix 0 is a bit-identical dry path")
    {
        LizardTapeAudioProcessor proc;
        setParam(proc.apvts, "mix",  0.f);
        setParam(proc.apvts, "hiss", 0.5f);
        proc.prepareToPlay(48000.0, 256);
        for (int n = 0; n < 8; ++n)
        {
            juce::AudioBuffer<float> buf(2, 256), dry(2, 256);
            fillWithSine(buf, 100.f, 48000.0);
            dry.makeCopyOf(buf);
            proc.processBlock(buf, midi);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < buf.getNumSamples(); ++i)
                    REQUIRE(buf.getSample(ch, i) == dry.getSample(ch, i));
        }
    }

    SECTION("silence in gives a low hiss floor out")
    {
        LizardTapeAudioProcessor proc;
        proc.prepareToPlay(48000.0, 512);
        float peak = 0.f;
        for (int n = 0; n < 20; ++n)
        {
            auto buf = makeSilent(2, 512);
            proc.processBlock(buf, midi);
            peak = std::max(peak, buf.getMagnitude(0, buf.getNumSamples()));
        }
        REQUIRE(peak > 0.f);
        REQUIRE(peak < 0.1f);
    }

    SECTION("mono layout is supported")
    {
        LizardTapeAudioProcessor proc;
        juce::AudioProcessor::BusesLayout mono;
        mono.inputBuses.add(juce::AudioChannelSet::mono());
        mono.outputBuses.add(juce::AudioChannelSet::mono());
        REQUIRE(proc.checkBusesLayoutSupported(mono));
    }
}

// ── State management tests ────────────────────────────────────────────────────

TEST_CASE("LizardTape - state management", "[lizard_tape]")
{
    SECTION("getStateInformation / setStateInformation round-trip")
    {
        LizardTapeAudioProcessor proc;
        setParam(proc.apvts, "drive",        15.f);
        setParam(proc.apvts, "wowDepth",     7.5f);
        setParam(proc.apvts, "flutterRate",  12.f);
        setParam(proc.apvts, "tone",         3000.f);
        setParam(proc.apvts, "mix",          0.4f);
        setParam(proc.apvts, "output",       -6.f);

        juce::MemoryBlock stateData;
        proc.getStateInformation(stateData);
        REQUIRE(stateData.getSize() > 0);

        LizardTapeAudioProcessor restored;
        restored.setStateInformation(stateData.getData(), (int)stateData.getSize());
        for (const char* id : kParamIDs)
        {
            INFO("Checking parameter: " << id);
            REQUIRE(getParam(restored.apvts, id) == Catch::Approx(getParam(proc.apvts, id)).margin(0.001f));
        }
        REQUIRE(getParam(restored.apvts, "tone") == Catch::Approx(3000.f).margin(1.f));
    }

    SECTION("foreign state is ignored")
    {
        LizardTapeAudioProcessor proc;
        setParam(proc.apvts, "drive", 10.f);
        juce::MemoryBlock junk;
        juce::AudioProcessor::copyXmlToBinary(juce::XmlElement("NOT_PARAMS"), junk);
        proc.setStateInformation(junk.getData(), (int)junk.getSize());
        REQUIRE(getParam(proc.apvts, "drive") == Catch::Approx(10.f).margin(0.01f));
    }
}
