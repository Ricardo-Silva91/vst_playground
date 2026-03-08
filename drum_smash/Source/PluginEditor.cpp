#include "PluginEditor.h"
#include "PluginProcessor.h"

struct KnobDef { const char* paramId; const char* label; };
static const KnobDef kKnobDefs[] =
{
    { "bitDepth",      "Bit Depth"    },
    { "sampleRateDiv", "SR Divide"    },
    { "drive",         "Drive"        },
    { "outputGain",    "Output Gain"  },
    { "noiseAmount",   "Noise"        },
    { "crackleRate",   "Crackle"      },
    { "lpfCutoff",     "LPF"          },
    { "hpfCutoff",     "HPF"          },
    { "compThreshold", "Threshold"    },
    { "compRatio",     "Ratio"        },
    { "compAttack",    "Attack"       },
    { "compRelease",   "Release"      },
    { "compMakeup",    "Makeup"       },
    { "reverbRoom",    "Room"         },
    { "reverbWet",     "Rev Wet"      },
    { "reverbDamping", "Damping"      },
    { "pitchSemitones","Pitch"        },
    { "wowRate",       "Wow Rate"     },
    { "wowDepth",      "Wow Depth"    },
    { "stereoWidth",   "Width"        },
    { "transientBoost","Transient"    },
};
static constexpr int kNumKnobs = (int)(sizeof(kKnobDefs) / sizeof(kKnobDefs[0]));

DrumSmashEditor::DrumSmashEditor (DrumSmashProcessor& p)
    : AudioProcessorEditor (p), proc (p)
{
    setSize (860, 420);

    // ── Preset buttons ────────────────────────────────────────────────────
    for (int i = 0; i < kNumPresets; ++i)
    {
        auto* btn = presetButtons.add (std::make_unique<juce::TextButton> (kPresets[i].name));
        btn->setClickingTogglesState (true);
        btn->setRadioGroupId (1);
        btn->onClick = [this, i] {
            selectedPreset = i;
            proc.setCurrentProgram (i);
        };
        addAndMakeVisible (btn);
    }

    // ── Knobs ─────────────────────────────────────────────────────────────
    for (int i = 0; i < kNumKnobs; ++i)
    {
        KnobRow row;
        row.slider = std::make_unique<juce::Slider> (juce::Slider::RotaryVerticalDrag,
                                                      juce::Slider::TextBoxBelow);
        row.label  = std::make_unique<juce::Label>  ("", kKnobDefs[i].label);
        row.label->setJustificationType (juce::Justification::centred);
        row.attach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
            (proc.apvts, kKnobDefs[i].paramId, *row.slider);
        addAndMakeVisible (*row.slider);
        addAndMakeVisible (*row.label);
        knobs.push_back (std::move (row));
    }

    selectedPreset = proc.getCurrentPresetIndex();
    if (selectedPreset >= 0 && selectedPreset < presetButtons.size())
        presetButtons[selectedPreset]->setToggleState (true, juce::dontSendNotification);

    startTimerHz (10);
}

DrumSmashEditor::~DrumSmashEditor()
{
    stopTimer();
}

void DrumSmashEditor::timerCallback()
{
    int curr = proc.getCurrentPresetIndex();
    if (curr != selectedPreset)
    {
        selectedPreset = curr;
        for (int i = 0; i < presetButtons.size(); ++i)
            presetButtons[i]->setToggleState (i == curr, juce::dontSendNotification);
    }
}

void DrumSmashEditor::resized()
{
    auto area = getLocalBounds().reduced (8);

    // Title strip
    area.removeFromTop (28);

    // Preset buttons — two rows of 4
    auto presetArea = area.removeFromTop (60);
    int bw = presetArea.getWidth() / 4;
    for (int i = 0; i < presetButtons.size(); ++i)
    {
        int row = i / 4;
        int col = i % 4;
        presetButtons[i]->setBounds (presetArea.getX() + col * bw,
                                      presetArea.getY() + row * 28,
                                      bw - 4, 24);
    }

    area.removeFromTop (8);

    // Knob grid — 7 columns x 3 rows
    const int cols = 7;
    const int rows = 3;
    int kw = area.getWidth()  / cols;
    int kh = area.getHeight() / rows;

    for (int i = 0; i < kNumKnobs; ++i)
    {
        int col = i / rows;
        int row = i % rows;
        juce::Rectangle<int> cell (area.getX() + col * kw,
                                    area.getY() + row * kh,
                                    kw, kh);
        knobs[(size_t)i].label ->setBounds (cell.removeFromTop (16));
        knobs[(size_t)i].slider->setBounds (cell.reduced (2));
    }
}

void DrumSmashEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
    g.setColour (juce::Colours::white);
    g.drawText ("Drum Smash", getLocalBounds().removeFromTop (28),
                juce::Justification::centred, false);
}

juce::AudioProcessorEditor* DrumSmashProcessor::createEditor()
{
    return new DrumSmashEditor (*this);
}