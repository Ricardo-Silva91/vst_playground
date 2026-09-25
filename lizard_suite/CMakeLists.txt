cmake_minimum_required(VERSION 3.15)
project(lizard_suite VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../JUCE JUCE_build)

juce_add_binary_data(LizardSuiteData
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/fonts/Rajdhani-Bold.ttf
        ${CMAKE_CURRENT_SOURCE_DIR}/fonts/ShareTechMono-Regular.ttf
        ${CMAKE_CURRENT_SOURCE_DIR}/logo_transparent.svg
)

juce_add_plugin(lizard_suite
    PLUGIN_MANUFACTURER_CODE LvsT
    PLUGIN_CODE LzSt
    FORMATS              VST3
    PRODUCT_NAME         "Lizard Suite"
    COMPANY_NAME         "lizard vst"
    IS_SYNTH             FALSE
    NEEDS_MIDI_INPUT     FALSE
    NEEDS_MIDI_OUTPUT    FALSE
    IS_MIDI_EFFECT       FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES      "Fx"
)

juce_generate_juce_header(lizard_suite)

target_sources(lizard_suite PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/SharedEditorUtils.cpp
)

target_include_directories(lizard_suite
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../shared
)

target_compile_definitions(lizard_suite PUBLIC
    _USE_MATH_DEFINES   # M_PI in the DSP cores on MSVC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(lizard_suite
    PRIVATE
        LizardSuiteData
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
