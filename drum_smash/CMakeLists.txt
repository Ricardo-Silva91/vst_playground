cmake_minimum_required(VERSION 3.15)
project(drum_smash VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../JUCE JUCE_build)

juce_add_binary_data(DrumSmashData
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/fonts/Rajdhani-Bold.ttf
        ${CMAKE_CURRENT_SOURCE_DIR}/fonts/ShareTechMono-Regular.ttf
        ${CMAKE_CURRENT_SOURCE_DIR}/logo_transparent.svg
)

juce_add_plugin(drum_smash
    PLUGIN_MANUFACTURER_CODE Anth
    PLUGIN_CODE Dsms
    FORMATS VST3
    PRODUCT_NAME "Drum Smash"
    COMPANY_NAME "Anthropic"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES "Fx"
)

juce_generate_juce_header(drum_smash)

target_sources(drum_smash PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_compile_definitions(drum_smash PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(drum_smash PRIVATE
    DrumSmashData
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)