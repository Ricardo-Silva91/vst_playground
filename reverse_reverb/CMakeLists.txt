cmake_minimum_required(VERSION 3.22)
project(ReverseReverb VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)

# --- Pull in JUCE from the shared submodule at the repo root ---
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../JUCE JUCE_build)

# --- Declare the plugin ---
juce_add_plugin(ReverseReverb
    COMPANY_NAME         "YourName"
    IS_SYNTH             FALSE
    NEEDS_MIDI_INPUT     FALSE
    NEEDS_MIDI_OUTPUT    FALSE
    IS_MIDI_EFFECT       FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Manu
    PLUGIN_CODE Rev1
    FORMATS              VST3
    PRODUCT_NAME         "ReverseReverb"
)

# --- Source files ---
target_sources(ReverseReverb
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
)

# --- JUCE modules this plugin needs ---
target_link_libraries(ReverseReverb
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# --- Compile definitions ---
target_compile_definitions(ReverseReverb
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)