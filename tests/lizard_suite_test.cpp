#include <catch2/catch_all.hpp>
#include "PluginProcessor.h"
#include "TestHelpers.h"

// Turn every module off: Dust Mix 0, Chew Depth 0, Murk Mix 0, Vinyl Amount 0.
static void allModulesOff (juce::AudioProcessorValueTreeState& apvts)
{
    setParam (apvts, "dustMix",     0.f);
    setParam (apvts, "chewDepth",   0.f);
    setParam (apvts, "murkMix",     0.f);
    setParam (apvts, "vinylAmount", 0.f);
}

static bool identical (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
{
    for (int ch = 0; ch < a.getNumChannels(); ++ch)
        for (int i = 0; i < a.getNumSamples(); ++i)
            if (a.getSample (ch, i) != b.getSample (ch, i))
                return false;
    return true;
}

// ── Parameter tests ───────────────────────────────────────────────────────────

TEST_CASE("LizardSuite - parameters", "[lizard_suite]")
{
    LizardSuiteProcessor proc;

    SECTION("all parameter IDs exist")
    {
        for (const char* id : { "dustRate", "dustBits", "dustLowpass", "dustDrive", "dustMix",
                                 "chewRate", "chewDepth", "chewSmooth",
                                 "murkRoom", "murkDamp", "murkMix",
                                 "vinylAge", "vinylAmount", "vinylSeed" })
        {
            INFO("Checking parameter: " << id);
            REQUIRE(proc.apvts.getParameter(id) != nullptr);
        }
    }

    SECTION("default values match the DSP cores")
    {
        REQUIRE(getParam(proc.apvts, "dustRate")    == Catch::Approx(26040.f).margin(1.f));
        REQUIRE(getParam(proc.apvts, "dustBits")    == Catch::Approx(12.f));
        REQUIRE(getParam(proc.apvts, "dustLowpass") == Catch::Approx(0.f));
        REQUIRE(getParam(proc.apvts, "dustDrive")   == Catch::Approx(1.f));
        REQUIRE(getParam(proc.apvts, "dustMix")     == Catch::Approx(1.f));
        REQUIRE(getParam(proc.apvts, "chewRate")    == Catch::Approx(3.f).margin(0.01f));
        REQUIRE(getParam(proc.apvts, "chewDepth")   == Catch::Approx(0.5f).margin(0.001f));
        REQUIRE(getParam(proc.apvts, "chewSmooth")  == Catch::Approx(8.f).margin(0.05f));
        REQUIRE(getParam(proc.apvts, "murkRoom")    == Catch::Approx(0.6f).margin(0.001f));
        REQUIRE(getParam(proc.apvts, "murkDamp")    == Catch::Approx(0.5f).margin(0.001f));
        REQUIRE(getParam(proc.apvts, "murkMix")     == Catch::Approx(0.3f).margin(0.001f));
        REQUIRE(getParam(proc.apvts, "vinylAge")    == Catch::Approx(0.4f).margin(0.001f));
        REQUIRE(getParam(proc.apvts, "vinylAmount") == Catch::Approx(0.5f).margin(0.001f));
        REQUIRE(getParam(proc.apvts, "vinylSeed")   == Catch::Approx(1.f));
    }

    SECTION("parameter ranges")
    {
        auto rangeOf = [&](const juce::String& id) {
            return dynamic_cast<juce::RangedAudioParameter*>(
                proc.apvts.getParameter(id))->getNormalisableRange();
        };
        REQUIRE(rangeOf("dustRate").start    == Catch::Approx(1000.f));
        REQUIRE(rangeOf("dustRate").end      == Catch::Approx(48000.f));
        REQUIRE(rangeOf("dustBits").start    == Catch::Approx(2.f));
        REQUIRE(rangeOf("dustBits").end      == Catch::Approx(16.f));
        REQUIRE(rangeOf("dustLowpass").end   == Catch::Approx(20000.f));
        REQUIRE(rangeOf("chewRate").end      == Catch::Approx(20.f));
        REQUIRE(rangeOf("chewSmooth").start  == Catch::Approx(1.f));
        REQUIRE(rangeOf("chewSmooth").end    == Catch::Approx(50.f));
        REQUIRE(rangeOf("vinylSeed").start   == Catch::Approx(1.f));
        REQUIRE(rangeOf("vinylSeed").end     == Catch::Approx(999.f));
    }
}

// ── Audio processing tests ────────────────────────────────────────────────────

TEST_CASE("LizardSuite - off settings are bit-identical dry", "[lizard_suite]")
{
    LizardSuiteProcessor proc;
    proc.setPlayConfigDetails(2, 2, 44100.0, 512);
    proc.prepareToPlay(44100.0, 512);
    juce::MidiBuffer midi;

    // Push the other modules to their most extreme settings, so only the
    // "off" controls can be responsible for a clean result.
    setParam(proc.apvts, "dustRate",    4000.f);
    setParam(proc.apvts, "dustBits",    4.f);
    setParam(proc.apvts, "dustDrive",   8.f);
    setParam(proc.apvts, "chewRate",    20.f);
    setParam(proc.apvts, "murkRoom",    1.f);
    setParam(proc.apvts, "vinylAge",    1.f);
    allModulesOff(proc.apvts);

    for (int block = 0; block < 20; ++block)
    {
        juce::AudioBuffer<float> in(2, 512);
        fillWithSine(in, 440.f + 37.f * (float)block, 44100.0, 0.5f);
        juce::AudioBuffer<float> out(in);
        proc.processBlock(out, midi);
        REQUIRE(identical(in, out));
    }
}

TEST_CASE("LizardSuite - each module is wired and its off switch restores dry", "[lizard_suite]")
{
    struct Case { const char* id; float onValue; };
    const Case cases[] = {
        { "dustMix",     1.f },
        { "chewDepth",   1.f },
        { "murkMix",     1.f },
        { "vinylAmount", 1.f },
    };

    for (const auto& c : cases)
    {
        INFO("Module control: " << c.id);
        LizardSuiteProcessor proc;
        proc.setPlayConfigDetails(2, 2, 44100.0, 512);
        proc.prepareToPlay(44100.0, 512);
        juce::MidiBuffer midi;

        allModulesOff(proc.apvts);
        setParam(proc.apvts, "dustBits", 4.f);
        setParam(proc.apvts, "chewRate", 20.f);
        setParam(proc.apvts, c.id, c.onValue);

        bool changed = false;
        for (int block = 0; block < 40; ++block)
        {
            juce::AudioBuffer<float> in(2, 512);
            fillWithSine(in, 440.f, 44100.0, 0.5f);
            juce::AudioBuffer<float> out(in);
            proc.processBlock(out, midi);
            REQUIRE(allFinite(out));
            changed = changed || !identical(in, out);
        }
        REQUIRE(changed);

        setParam(proc.apvts, c.id, 0.f);
        juce::AudioBuffer<float> in(2, 512);
        fillWithSine(in, 440.f, 44100.0, 0.5f);
        juce::AudioBuffer<float> out(in);
        proc.processBlock(out, midi);
        REQUIRE(identical(in, out));
    }
}

TEST_CASE("LizardSuite - Murk stays stable at max Room", "[lizard_suite]")
{
    LizardSuiteProcessor proc;
    proc.setPlayConfigDetails(2, 2, 44100.0, 512);
    proc.prepareToPlay(44100.0, 512);
    juce::MidiBuffer midi;

    allModulesOff(proc.apvts);
    setParam(proc.apvts, "murkRoom", 1.f);
    setParam(proc.apvts, "murkDamp", 0.f);
    setParam(proc.apvts, "murkMix",  1.f);

    juce::Random rng (12345);
    for (int block = 0; block < 400; ++block)   // ~4.6 s of sustained noise
    {
        juce::AudioBuffer<float> buf(2, 512);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i)
                buf.setSample(ch, i, (rng.nextFloat() * 2.f - 1.f) * 0.5f);
        proc.processBlock(buf, midi);
        REQUIRE(allFinite(buf));
        REQUIRE(allBounded(buf, 8.f));
    }
}

TEST_CASE("LizardSuite - Chew is pure gain and linked across channels", "[lizard_suite]")
{
    LizardSuiteProcessor proc;
    proc.setPlayConfigDetails(2, 2, 44100.0, 512);
    proc.prepareToPlay(44100.0, 512);
    juce::MidiBuffer midi;

    allModulesOff(proc.apvts);
    setParam(proc.apvts, "chewDepth", 1.f);
    setParam(proc.apvts, "chewRate",  20.f);

    float minGain = 1.f;
    for (int block = 0; block < 200; ++block)
    {
        juce::AudioBuffer<float> buf(2, 512);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i)
                buf.setSample(ch, i, 1.f);          // DC in -> output is the gain envelope
        proc.processBlock(buf, midi);
        for (int i = 0; i < 512; ++i)
        {
            const float l = buf.getSample(0, i);
            REQUIRE(l <= 1.0001f);
            REQUIRE(l == buf.getSample(1, i));
            minGain = std::min(minGain, l);
        }
    }
    REQUIRE(minGain < 0.9f);
}

TEST_CASE("LizardSuite - Vinyl bed", "[lizard_suite]")
{
    auto renderBed = [](int seed, int blocks)
    {
        LizardSuiteProcessor proc;
        proc.setPlayConfigDetails(2, 2, 44100.0, 512);
        allModulesOff(proc.apvts);
        setParam(proc.apvts, "vinylAmount", 1.f);
        setParam(proc.apvts, "vinylAge",    0.8f);
        setParam(proc.apvts, "vinylSeed",   (float)seed);
        proc.prepareToPlay(44100.0, 512);
        juce::MidiBuffer midi;

        juce::AudioBuffer<float> all(2, 512 * blocks);
        for (int b = 0; b < blocks; ++b)
        {
            auto buf = makeSilent(2, 512);
            proc.processBlock(buf, midi);
            for (int ch = 0; ch < 2; ++ch)
                all.copyFrom(ch, b * 512, buf, ch, 0, 512);
        }
        return all;
    };

    SECTION("silence in -> finite, non-zero bed")
    {
        auto bed = renderBed(1, 20);
        REQUIRE(allFinite(bed));
        REQUIRE(rmsOf(bed) > 1e-4f);
        REQUIRE(allBounded(bed, 2.f));
    }

    SECTION("same seed -> identical output; new seed -> new output")
    {
        auto a = renderBed(7, 10);
        auto b = renderBed(7, 10);
        auto c = renderBed(8, 10);
        REQUIRE(identical(a, b));
        REQUIRE(!identical(a, c));
    }

    SECTION("left and right hiss are different streams")
    {
        auto bed = renderBed(1, 4);
        bool differs = false;
        for (int i = 0; i < bed.getNumSamples() && !differs; ++i)
            differs = bed.getSample(0, i) != bed.getSample(1, i);
        REQUIRE(differs);
    }

    SECTION("bed does not repeat every block")
    {
        auto bed = renderBed(1, 4);
        bool repeats = true;
        for (int i = 0; i < 512 && repeats; ++i)
            repeats = bed.getSample(0, i) == bed.getSample(0, 512 + i);
        REQUIRE(!repeats);
    }
}

TEST_CASE("LizardSuite - lifecycle and layout", "[lizard_suite]")
{
    LizardSuiteProcessor proc;

    SECTION("prepareToPlay can be called multiple times and at other rates")
    {
        proc.setPlayConfigDetails(2, 2, 44100.0, 512);
        proc.prepareToPlay(44100.0, 512);
        proc.prepareToPlay(96000.0, 256);
        proc.prepareToPlay(48000.0, 1024);
        juce::MidiBuffer midi;
        juce::AudioBuffer<float> buf(2, 1024);
        fillWithSine(buf, 440.f, 48000.0, 0.5f);
        proc.processBlock(buf, midi);
        REQUIRE(allFinite(buf));
        REQUIRE(allBounded(buf, 3.f));
    }

    SECTION("mono and stereo layouts are supported, mismatched ones are not")
    {
        juce::AudioProcessor::BusesLayout mono, stereo, mixed;
        mono.inputBuses.add(juce::AudioChannelSet::mono());
        mono.outputBuses.add(juce::AudioChannelSet::mono());
        stereo.inputBuses.add(juce::AudioChannelSet::stereo());
        stereo.outputBuses.add(juce::AudioChannelSet::stereo());
        mixed.inputBuses.add(juce::AudioChannelSet::mono());
        mixed.outputBuses.add(juce::AudioChannelSet::stereo());
        REQUIRE(proc.checkBusesLayoutSupported(mono));
        REQUIRE(proc.checkBusesLayoutSupported(stereo));
        REQUIRE(!proc.checkBusesLayoutSupported(mixed));
    }

    SECTION("state save / restore round-trips parameters")
    {
        setParam(proc.apvts, "murkRoom",  0.9f);
        setParam(proc.apvts, "vinylSeed", 42.f);
        juce::MemoryBlock state;
        proc.getStateInformation(state);

        LizardSuiteProcessor proc2;
        proc2.setStateInformation(state.getData(), (int)state.getSize());
        REQUIRE(getParam(proc2.apvts, "murkRoom")  == Catch::Approx(0.9f).margin(0.001f));
        REQUIRE(getParam(proc2.apvts, "vinylSeed") == Catch::Approx(42.f));
    }
}
