cmake_minimum_required(VERSION 3.22)
project(pitch_wobble VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)

# JUCE lives at repo root, one level up from this plugin folder
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../JUCE JUCE_build)

# Embed fonts and logo as binary data
juce_add_binary_data(PitchWobbleFonts
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/fonts/Rajdhani-Bold.ttf
        ${CMAKE_CURRENT_SOURCE_DIR}/fonts/ShareTechMono-Regular.ttf
        ${CMAKE_CURRENT_SOURCE_DIR}/logo_transparent.svg
)

juce_add_plugin(pitch_wobble
    COMPANY_NAME         "YourName"
    IS_SYNTH             FALSE
    NEEDS_MIDI_INPUT     FALSE
    NEEDS_MIDI_OUTPUT    FALSE
    IS_MIDI_EFFECT       FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Mypg
    PLUGIN_CODE Pwob
    FORMATS              VST3
    PRODUCT_NAME         "Pitch Wobble"
)

target_sources(pitch_wobble
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
)

target_link_libraries(pitch_wobble
    PRIVATE
        PitchWobbleFonts
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

target_compile_definitions(pitch_wobble
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)