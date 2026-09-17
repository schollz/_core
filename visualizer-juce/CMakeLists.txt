cmake_minimum_required(VERSION 3.22)
project(zeptocore_visualizer VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
include(FetchContent)
set(FETCHCONTENT_BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.cache/deps")
set(juce_archive https://github.com/juce-framework/JUCE/archive/refs/tags/9.0.2.tar.gz)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.cache/JUCE-9.0.2.tar.gz")
    set(juce_archive "${CMAKE_CURRENT_SOURCE_DIR}/.cache/JUCE-9.0.2.tar.gz")
endif()
FetchContent_Declare(JUCE URL "${juce_archive}"
    URL_HASH SHA256=16d01c27e8327f3644306cc5c223aa6e21af1cc1ec62faa709fc0dced57d208a
    BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/juce-framework"
    SUBBUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/juce-fetch"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(JUCE)
file(STRINGS "${juce_SOURCE_DIR}/CMakeLists.txt" juce_version REGEX "^project\\(JUCE VERSION")
if(NOT juce_version MATCHES "VERSION 9\\.0\\.2 ")
    message(FATAL_ERROR "Requires exactly JUCE 9.0.2")
endif()
set(formats Standalone VST3)
if(APPLE)
    list(APPEND formats AU)
endif()
juce_add_plugin(zeptocore_visualizer
    COMPANY_NAME "INFINITE DIGITS"
    BUNDLE_ID "com.infinitedigits.zeptocorevisualizer"
    PLUGIN_MANUFACTURER_CODE INFD PLUGIN_CODE ZpVw
    PRODUCT_NAME "zeptocore visualizer"
    DESCRIPTION "Zeptocore reference waveform and performance monitor"
    FORMATS ${formats} IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE NEEDS_MIDI_OUTPUT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Analyzer)
juce_add_binary_data(VisualizerAssets SOURCES
    Resources/Fonts/IBMPlexMono-Regular.ttf
    Resources/Fonts/IBMPlexMono-Medium.ttf
    Resources/Fonts/IBMPlexMono-Bold.ttf
    Resources/Fonts/fa-solid-900.ttf)
set_target_properties(VisualizerAssets PROPERTIES
    CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN YES)
target_sources(zeptocore_visualizer PRIVATE
    Source/Core.cpp Source/Library.cpp Source/Midi.cpp Source/Session.cpp
    Source/PluginProcessor.cpp Source/VisualizerView.cpp)
target_include_directories(zeptocore_visualizer PUBLIC Source)
target_compile_definitions(zeptocore_visualizer PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1)
target_link_libraries(zeptocore_visualizer PRIVATE VisualizerAssets
    juce::juce_audio_utils juce::juce_cryptography
    PUBLIC juce::juce_recommended_config_flags)
target_sources(zeptocore_visualizer_Standalone PRIVATE Source/Main.cpp Tests/Tests.cpp Tests/MidiTests.cpp)
juce_add_binary_data(VisualizerFixtures HEADER_NAME FixtureData.h NAMESPACE FixtureData SOURCES Tests/parity.json)
set_target_properties(VisualizerFixtures PROPERTIES
    CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN YES)
target_link_libraries(zeptocore_visualizer_Standalone PRIVATE VisualizerFixtures)
# Analysis should remain responsive in developer builds too.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    set_source_files_properties(Source/Core.cpp PROPERTIES COMPILE_OPTIONS "-O2")
endif()
if(APPLE)
    enable_language(OBJCXX)
    # CMake's CXX visibility property does not apply to Objective-C++. Without
    # this, JUCE implementation symbols can interpose with a JUCE-based host
    # (including Debug/Release String/var internals) and corrupt ownership.
    set_target_properties(zeptocore_visualizer PROPERTIES
        OBJCXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN YES)
    target_sources(zeptocore_visualizer PRIVATE Source/Platform_mac.mm)
    set(notices LICENSE Resources/Fonts/IBM-Plex-LICENSE.txt Resources/Fonts/Font-Awesome-LICENSE.txt "${juce_SOURCE_DIR}/LICENSE.md")
    set_source_files_properties(${notices} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    foreach(format IN LISTS formats)
        set_target_properties(zeptocore_visualizer_${format} PROPERTIES
            OBJCXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN YES)
        target_sources(zeptocore_visualizer_${format} PRIVATE ${notices})
        add_custom_command(TARGET zeptocore_visualizer_${format} POST_BUILD
            COMMAND /usr/bin/codesign --force --sign - "$<TARGET_BUNDLE_DIR:zeptocore_visualizer_${format}>"
            COMMAND /usr/bin/codesign --verify --deep --strict "$<TARGET_BUNDLE_DIR:zeptocore_visualizer_${format}>"
            VERBATIM)
    endforeach()
else()
    target_sources(zeptocore_visualizer PRIVATE Source/Platform.cpp)
endif()
enable_testing()
add_test(NAME visualizer_contract COMMAND zeptocore_visualizer_Standalone --self-test)
set_tests_properties(visualizer_contract PROPERTIES TIMEOUT 120 FAIL_REGULAR_EXPRESSION "JUCE Assertion failure")
if(APPLE)
    add_test(NAME visualizer_native_midi COMMAND zeptocore_visualizer_Standalone --midi-test)
    set_tests_properties(visualizer_native_midi PROPERTIES TIMEOUT 20 FAIL_REGULAR_EXPRESSION "JUCE Assertion failure")
endif()

# Optional real plugin-wrapper probe; not linked into delivered app/plugin bundles.
option(VISUALIZER_PLUGIN_PROBE "Build the local plugin validation host" OFF)
if(VISUALIZER_PLUGIN_PROBE)
    juce_add_console_app(visualizer_plugin_probe PRODUCT_NAME "visualizer plugin probe")
    set_target_properties(visualizer_plugin_probe PROPERTIES
        CXX_VISIBILITY_PRESET hidden OBJCXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN YES)
    target_sources(visualizer_plugin_probe PRIVATE Tests/PluginProbe.cpp)
    target_compile_definitions(visualizer_plugin_probe PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_PLUGINHOST_VST3=1 JUCE_PLUGINHOST_AU=$<BOOL:${APPLE}>)
    target_link_libraries(visualizer_plugin_probe PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)
endif()
