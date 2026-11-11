cmake_minimum_required(VERSION 3.22)
project(LiveVoice VERSION 0.1.5)
# Version shown in the app and in file names (0.1, 0.11, 0.12 ...)
set(LIVEVOICE_LABEL "0.15")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")   # no VC++ redistributable needed
endif()

add_subdirectory(JUCE)

juce_add_binary_data(LiveVoiceData HEADER_NAME BinaryData.h NAMESPACE BinaryData
    SOURCES Resources/fonts/InterRegular.otf Resources/fonts/InterSemiBold.otf)
set_target_properties(LiveVoiceData PROPERTIES POSITION_INDEPENDENT_CODE ON)

juce_add_plugin(LiveVoice
    COMPANY_NAME "Thematic"
    PLUGIN_MANUFACTURER_CODE Thmt
    PLUGIN_CODE LvVc
    FORMATS Standalone
    PRODUCT_NAME "LIVE VOICE"
    BUNDLE_ID com.thematic.livevoice
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "LIVE VOICE processes your microphone in real time."
    ICON_BIG Resources/icon.png
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE)

target_sources(LiveVoice PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/StandaloneApp.cpp)

target_include_directories(LiveVoice PRIVATE third_party)

target_compile_definitions(LiveVoice PUBLIC
    LIVEVOICE_LABEL="${LIVEVOICE_LABEL}"
    JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    _USE_MATH_DEFINES=1)

if(MSVC)
    target_compile_options(LiveVoice PRIVATE /utf-8)
endif()

target_link_libraries(LiveVoice
    PRIVATE
        LiveVoiceData
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Offscreen renderer: CI screenshots of the UI
juce_add_console_app(Snapshot PRODUCT_NAME "Snapshot")
target_sources(Snapshot PRIVATE tools/Snapshot.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_include_directories(Snapshot PRIVATE third_party)
target_compile_definitions(Snapshot PRIVATE LIVEVOICE_LABEL="${LIVEVOICE_LABEL}" JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1 _USE_MATH_DEFINES=1)
if(MSVC)
    target_compile_options(Snapshot PRIVATE /utf-8)
endif()
target_link_libraries(Snapshot PRIVATE LiveVoiceData juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)

# DSP tests (no JUCE)
add_executable(DspTest test/dsp_test.cpp)
target_include_directories(DspTest PRIVATE Source third_party)
target_compile_definitions(DspTest PRIVATE _USE_MATH_DEFINES=1)
