cmake_minimum_required(VERSION 3.22)
project(CA2020 VERSION 1.0.0)

# Static MSVC runtime: no separate Visual C++ redistributable needed on Windows
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")

# JUCE is cloned into ./JUCE by the GitHub workflow
add_subdirectory(JUCE)

juce_add_plugin(CA2020
    COMPANY_NAME "Landgren Audio"
    BUNDLE_ID "com.landgrenaudio.ca2020"
    PLUGIN_MANUFACTURER_CODE Lgra
    PLUGIN_CODE Ca20
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "CA-2020"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Generator
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    ICON_BIG Resources/icon.png
    ICON_SMALL Resources/icon.png)

juce_generate_juce_header(CA2020)

target_sources(CA2020 PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_include_directories(CA2020 PRIVATE Source Source/engine)

target_compile_definitions(CA2020 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_STANDALONE_FILTER_WINDOW_USE_KIOSK_MODE=0)

target_link_libraries(CA2020
    PRIVATE juce::juce_audio_utils
    PUBLIC  juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)
