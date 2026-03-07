cmake_minimum_required(VERSION 3.22)
project(ReverseReverb VERSION 1.0.0)

# --- Pull in JUCE (added as a git submodule) ---
add_subdirectory(JUCE)

# --- Declare the plugin ---
juce_add_plugin(ReverseReverb
    COMPANY_NAME         "YourName"
    IS_SYNTH             FALSE          # it's an effect, not an instrument
    NEEDS_MIDI_INPUT     FALSE
    NEEDS_MIDI_OUTPUT    FALSE
    IS_MIDI_EFFECT       FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE       # set TRUE to auto-install locally
    PLUGIN_MANUFACTURER_CODE Manu
    PLUGIN_CODE Rev1
    FORMATS              VST3           # build VST3 only (add AU for Mac if needed)
    PRODUCT_NAME         "ReverseReverb"
)

# --- Source files ---
target_sources(ReverseReverb
    PRIVATE
        PluginProcessor.cpp
        PluginProcessor.h
        PluginEditor.cpp
        PluginEditor.h
)

# --- JUCE modules this plugin needs ---
target_link_libraries(ReverseReverb
    PRIVATE
        juce::juce_audio_utils      # AudioProcessorEditor, sliders, etc.
        juce::juce_dsp              # DSP utilities (reverb, filters, etc.)
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# --- Compile definitions ---
target_compile_definitions(ReverseReverb
    PUBLIC
        JUCE_WEB_BROWSER=0          # we don't need a browser component
        JUCE_USE_CURL=0             # we don't need curl
        JUCE_VST3_CAN_REPLACE_VST2=0
)
