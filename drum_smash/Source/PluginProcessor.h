#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

// ── Preset definitions ────────────────────────────────────────────────────────
struct DrumPreset
{
    const char* name;

    // Bit-crusher
    float bitDepth;       // 1-16
    float sampleRateDiv;  // 1-32 (integer downsampling factor)

    // Saturation / drive
    float drive;          // 0-1
    float outputGain;     // linear multiplier

    // Dust / vinyl noise
    float noiseAmount;    // 0-1
    float crackleRate;    // 0-1  (density of crackle events)

    // Low-pass filter (dusty roll-off)
    float lpfCutoff;      // Hz  (20-22000)

    // High-pass filter (thin / radio)
    float hpfCutoff;      // Hz

    // Compression (make it hit harder)
    float compThresholdDb; // -60 to 0
    float compRatio;       // 1 to 20
    float compAttackMs;
    float compReleaseMs;
    float compMakeupDb;

    // Reverb (room / space)
    float reverbRoomSize;  // 0-1
    float reverbWet;       // 0-1
    float reverbDamping;   // 0-1

    // Pitch / tape-flutter
    float pitchSemitones;  // -12 to +12
    float wowFlutterRate;  // Hz  (0 = off)
    float wowFlutterDepth; // cents

    // Stereo width
    float stereoWidth;     // 0-2  (1 = normal)

    // Transient punch (attack boost via envelope)
    float transientBoost;  // 0-1
};

static const DrumPreset kPresets[] =
{
    // 0 – Dusty Vinyl
    { "Dusty Vinyl",
      /*bit*/16, /*srDiv*/1,
      /*drive*/0.1f, /*outGain*/1.0f,
      /*noise*/0.35f, /*crackle*/0.4f,
      /*lpf*/5000.f, /*hpf*/80.f,
      /*compThr*/-18.f, /*ratio*/3.f, /*att*/10.f, /*rel*/150.f, /*mkup*/4.f,
      /*revRoom*/0.3f, /*revWet*/0.08f, /*revDamp*/0.8f,
      /*pitch*/0.f, /*wowRate*/0.6f, /*wowDepth*/8.f,
      /*width*/0.9f, /*transient*/0.0f },

    // 1 – Heavy Hitter
    { "Heavy Hitter",
      /*bit*/16, /*srDiv*/1,
      /*drive*/0.65f, /*outGain*/1.3f,
      /*noise*/0.0f, /*crackle*/0.0f,
      /*lpf*/18000.f, /*hpf*/60.f,
      /*compThr*/-24.f, /*ratio*/8.f, /*att*/1.f, /*rel*/80.f, /*mkup*/9.f,
      /*revRoom*/0.1f, /*revWet*/0.04f, /*revDamp*/0.5f,
      /*pitch*/0.f, /*wowRate*/0.f, /*wowDepth*/0.f,
      /*width*/1.1f, /*transient*/0.85f },

    // 2 – Bit Crusher
    { "Bit Crusher",
      /*bit*/4, /*srDiv*/8,
      /*drive*/0.4f, /*outGain*/1.0f,
      /*noise*/0.05f, /*crackle*/0.0f,
      /*lpf*/8000.f, /*hpf*/100.f,
      /*compThr*/-12.f, /*ratio*/4.f, /*att*/5.f, /*rel*/100.f, /*mkup*/3.f,
      /*revRoom*/0.0f, /*revWet*/0.0f, /*revDamp*/0.5f,
      /*pitch*/0.f, /*wowRate*/0.f, /*wowDepth*/0.f,
      /*width*/1.0f, /*transient*/0.0f },

    // 3 – Lo-Fi Radio
    { "Lo-Fi Radio",
      /*bit*/10, /*srDiv*/3,
      /*drive*/0.2f, /*outGain*/1.0f,
      /*noise*/0.15f, /*crackle*/0.1f,
      /*lpf*/3500.f, /*hpf*/300.f,
      /*compThr*/-14.f, /*ratio*/5.f, /*att*/8.f, /*rel*/120.f, /*mkup*/5.f,
      /*revRoom*/0.15f, /*revWet*/0.05f, /*revDamp*/0.7f,
      /*pitch*/0.f, /*wowRate*/1.2f, /*wowDepth*/12.f,
      /*width*/0.6f, /*transient*/0.0f },

    // 4 – Punchy Club
    { "Punchy Club",
      /*bit*/16, /*srDiv*/1,
      /*drive*/0.3f, /*outGain*/1.1f,
      /*noise*/0.0f, /*crackle*/0.0f,
      /*lpf*/20000.f, /*hpf*/40.f,
      /*compThr*/-20.f, /*ratio*/6.f, /*att*/2.f, /*rel*/60.f, /*mkup*/7.f,
      /*revRoom*/0.4f, /*revWet*/0.18f, /*revDamp*/0.3f,
      /*pitch*/0.f, /*wowRate*/0.f, /*wowDepth*/0.f,
      /*width*/1.3f, /*transient*/0.6f },

    // 5 – Tape Warped
    { "Tape Warped",
      /*bit*/14, /*srDiv*/1,
      /*drive*/0.45f, /*outGain*/1.05f,
      /*noise*/0.2f, /*crackle*/0.2f,
      /*lpf*/7000.f, /*hpf*/60.f,
      /*compThr*/-16.f, /*ratio*/4.f, /*att*/6.f, /*rel*/200.f, /*mkup*/4.f,
      /*revRoom*/0.35f, /*revWet*/0.12f, /*revDamp*/0.6f,
      /*pitch*/0.f, /*wowRate*/2.5f, /*wowDepth*/20.f,
      /*width*/1.0f, /*transient*/0.2f },

    // 6 – Nuclear Snare
    { "Nuclear Snare",
      /*bit*/16, /*srDiv*/1,
      /*drive*/0.8f, /*outGain*/1.4f,
      /*noise*/0.05f, /*crackle*/0.0f,
      /*lpf*/16000.f, /*hpf*/80.f,
      /*compThr*/-30.f, /*ratio*/15.f, /*att*/0.5f, /*rel*/50.f, /*mkup*/12.f,
      /*revRoom*/0.5f, /*revWet*/0.22f, /*revDamp*/0.4f,
      /*pitch*/0.f, /*wowRate*/0.f, /*wowDepth*/0.f,
      /*width*/1.4f, /*transient*/1.0f },

    // 7 – Bedroom Boom-Bap
    { "Bedroom Boom-Bap",
      /*bit*/12, /*srDiv*/2,
      /*drive*/0.25f, /*outGain*/1.0f,
      /*noise*/0.25f, /*crackle*/0.3f,
      /*lpf*/9000.f, /*hpf*/100.f,
      /*compThr*/-22.f, /*ratio*/5.f, /*att*/4.f, /*rel*/140.f, /*mkup*/6.f,
      /*revRoom*/0.2f, /*revWet*/0.07f, /*revDamp*/0.75f,
      /*pitch*/-0.3f, /*wowRate*/0.8f, /*wowDepth*/6.f,
      /*width*/0.8f, /*transient*/0.3f },
};

static constexpr int kNumPresets = (int)(sizeof(kPresets) / sizeof(kPresets[0]));

// ── Processor ─────────────────────────────────────────────────────────────────
class DrumSmashProcessor : public juce::AudioProcessor
{
public:
    DrumSmashProcessor();
    ~DrumSmashProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Drum Smash"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return kNumPresets; }
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // Expose for editor
    int getCurrentPresetIndex() const { return currentPreset; }

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void applyPreset (int index);
    void rebuildDSP();

    int currentPreset = 0;
    double currentSampleRate = 44100.0;

    // DSP objects
    juce::dsp::ProcessorChain<
        juce::dsp::IIR::Filter<float>,   // 0 HPF
        juce::dsp::IIR::Filter<float>    // 1 LPF
    > filterChain;

    juce::dsp::Compressor<float> compressor;
    juce::dsp::Reverb reverb;

    // Bit-crusher state
    float bcPhase = 0.f;
    float bcHeldL = 0.f, bcHeldR = 0.f;

    // Wow/flutter LFO
    float wowPhase = 0.f;

    // Transient shaper (simple envelope follower)
    float envFollower = 0.f;

    // Noise seed
    juce::Random rng;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DrumSmashProcessor)
};
