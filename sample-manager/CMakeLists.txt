cmake_minimum_required(VERSION 3.22)
set(CORE_MANAGER_VERSION "0.1.0" CACHE STRING "Independent application version")
project(CoreSampleManager VERSION ${CORE_MANAGER_VERSION} LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
include(FetchContent)
set(FETCHCONTENT_BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.cache/deps")
set(juce_archive https://github.com/juce-framework/JUCE/archive/refs/tags/9.0.3.tar.gz)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.cache/JUCE-9.0.3.tar.gz")
    set(juce_archive "${CMAKE_CURRENT_SOURCE_DIR}/.cache/JUCE-9.0.3.tar.gz")
endif()
FetchContent_Declare(JUCE URL "${juce_archive}"
    URL_HASH SHA256=a81e5508b8a0efa483917794ebeaff56aed3075730c405a947734413f40c1aba
    BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/juce-framework"
    SUBBUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/juce-fetch" DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(JUCE)
file(STRINGS "${juce_SOURCE_DIR}/CMakeLists.txt" juce_version REGEX "^project\\(JUCE VERSION")
if(NOT juce_version MATCHES "VERSION 9\\.0\\.3 ")
    message(FATAL_ERROR "_core sample manager requires JUCE 9.0.3")
endif()
include(cmake/RubberBand.cmake)
include(cmake/FirmwareCatalog.cmake)
set(firmware_readme "${CMAKE_CURRENT_SOURCE_DIR}/../README.md")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${firmware_readme}")
core_firmware_catalog("${firmware_readme}" "${CMAKE_CURRENT_BINARY_DIR}/generated/FirmwareCatalogData.h")
add_library(core_tempo STATIC
    Vendor/soundtouch/source/SoundTouch/BPMDetect.cpp
    Vendor/soundtouch/source/SoundTouch/FIFOSampleBuffer.cpp
    Vendor/soundtouch/source/SoundTouch/PeakFinder.cpp)
target_include_directories(core_tempo PUBLIC Vendor/soundtouch/include)
target_compile_definitions(core_tempo PUBLIC SOUNDTOUCH_FLOAT_SAMPLES=1)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS Resources/Icons/app-icon.png)
juce_add_gui_app(CoreSampleManager COMPANY_NAME "INFINITE DIGITS"
    BUNDLE_ID "com.infinitedigits.coresamplemanager" PRODUCT_NAME "_core sample manager"
    ICON_BIG Resources/Icons/app-icon.png
    MICROPHONE_PERMISSION_ENABLED FALSE NEEDS_CURL TRUE)
juce_add_binary_data(CoreAssets SOURCES
    Resources/Fonts/IBMPlexMono-Regular.ttf Resources/Fonts/IBMPlexMono-Medium.ttf Resources/Fonts/IBMPlexMono-Bold.ttf
    Resources/Fonts/fa-solid-900.ttf Resources/Fonts/MonaspaceKrypton-Regular.otf Resources/Fonts/odin-rounded.regular.otf
    Resources/Artwork/rune1.svg Resources/Artwork/rune2.svg Resources/Artwork/rune3.svg Resources/Artwork/rune4.svg Resources/Artwork/rune5.svg Resources/Artwork/rune6.svg Resources/Artwork/rune7.svg Resources/Artwork/ectocore.png)
file(GLOB lucide_icons CONFIGURE_DEPENDS Resources/Icons/Lucide/*.svg)
juce_add_binary_data(CoreIcons HEADER_NAME LucideData.h NAMESPACE LucideData SOURCES
    ${lucide_icons} Resources/Icons/Lucide/Lucide-LICENSE.txt)
file(GLOB onset_sources CONFIGURE_DEPENDS Vendor/amenbreak/Onsets/*.cpp)
list(FILTER onset_sources EXCLUDE REGEX "(ExampleUsage|SliceAnalyzer)\\.cpp$")
juce_add_binary_data(CoreFixtures HEADER_NAME FixtureData.h NAMESPACE FixtureData SOURCES Tests/parity.json Tests/fixtures/website-name.json Tests/fixtures/website-1-44100.info Tests/fixtures/website-1-88200.info Tests/fixtures/website-2-44100.info Tests/fixtures/website-2-88200.info)
juce_add_binary_data(CoreImportFixtures HEADER_NAME ImportData.h NAMESPACE ImportData SOURCES
    Tests/fixtures/import.wav Tests/fixtures/import.flac Tests/fixtures/import.mp3 Tests/fixtures/import.ogg Tests/fixtures/import.xrni
    Tests/fixtures/op1.aif Tests/fixtures/custom.aiff Tests/fixtures/cue.wav Tests/fixtures/loop.wav)
juce_add_binary_data(CoreTempoFixtures HEADER_NAME TempoData.h NAMESPACE TempoData SOURCES
    Tests/fixtures/tempo/135-clean.wav Tests/fixtures/tempo/135-comp.wav Tests/fixtures/tempo/135-vintage.wav)
target_link_libraries(CoreSampleManager PRIVATE CoreFixtures CoreImportFixtures CoreTempoFixtures)
target_sources(CoreSampleManager PRIVATE Source/Main.cpp Source/CardFormat.cpp Source/Project.cpp Source/NameMetadata.cpp Source/Tempo.cpp Source/IconButton.cpp
    Source/Storage.cpp Source/AudioProcessing.cpp Source/Manager.cpp Source/Preview.cpp Source/Theme.cpp Source/WaveformEditor.cpp Source/SettingsView.cpp Source/AppView.cpp Source/Device.cpp Source/DeviceView.cpp Source/Uf2.cpp Source/OnlineAnalysis.cpp Source/Visualizer/Core.cpp Source/Visualizer/Library.cpp Source/Visualizer/Midi.cpp Source/Visualizer/Session.cpp Source/Visualizer/VisualizerView.cpp Tests/VisualizerTests.cpp Tests/Tests.cpp Tests/ImportTests.cpp Tests/AudioTests.cpp Tests/TempoTests.cpp Tests/RecoveryTests.cpp Tests/ManagerTests.cpp Tests/MidiTests.cpp ${onset_sources})
target_sources(CoreSampleManager PRIVATE Source/Firmware.cpp Source/FirmwareReleases.cpp Source/FirmwareView.cpp Tests/FirmwareTests.cpp)
target_sources(CoreSampleManager PRIVATE Tests/RenderStatusTests.cpp)
target_sources(CoreSampleManager PRIVATE Tests/BatchImportTests.cpp)
target_include_directories(CoreSampleManager PRIVATE Source Vendor/amenbreak "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_compile_definitions(CoreSampleManager PRIVATE JUCE_WEB_BROWSER=0
    JUCE_USE_MP3AUDIOFORMAT=1 JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:CoreSampleManager,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="${PROJECT_VERSION}")
target_link_libraries(CoreSampleManager PRIVATE CoreAssets CoreIcons core_rubberband core_tempo
    juce::juce_audio_utils juce::juce_dsp juce::juce_cryptography juce::juce_recommended_config_flags)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions(CoreSampleManager PRIVATE JUCE_LOAD_CURL_SYMBOLS_LAZILY=0)
else()
    target_compile_definitions(CoreSampleManager PRIVATE JUCE_USE_CURL=0)
endif()
if(APPLE)
    enable_language(OBJCXX)
    target_sources(CoreSampleManager PRIVATE Source/Visualizer/Platform_mac.mm)
    # Icon-only changes must relink so the post-build signature covers the updated bundle.
    set_property(TARGET CoreSampleManager APPEND PROPERTY LINK_DEPENDS
        "$<TARGET_PROPERTY:CoreSampleManager,JUCE_ICON_FILE>")
    set(notices LICENSE Vendor/PROVENANCE.md Vendor/rubberband/COPYING Vendor/soundtouch/COPYING.TXT
        Resources/Fonts/IBM-Plex-LICENSE.txt Resources/Fonts/Font-Awesome-LICENSE.txt Resources/Fonts/Website-Fonts-NOTICE.txt Resources/Fonts/Monaspace-LICENSE.txt Resources/Icons/Lucide/Lucide-LICENSE.txt "${juce_SOURCE_DIR}/LICENSE.md")
    set_source_files_properties(${notices} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    target_sources(CoreSampleManager PRIVATE ${notices})
    add_custom_command(TARGET CoreSampleManager POST_BUILD
        COMMAND /usr/bin/codesign --force --sign - "$<TARGET_BUNDLE_DIR:CoreSampleManager>" VERBATIM)
else()
    target_sources(CoreSampleManager PRIVATE Source/Visualizer/Platform.cpp)
endif()
enable_testing()
add_test(NAME batch_import COMMAND CoreSampleManager --self-test batch-import)
set_tests_properties(batch_import PROPERTIES TIMEOUT 120 FAIL_REGULAR_EXPRESSION "JUCE Assertion failure")
add_test(NAME companion_migration COMMAND CoreSampleManager --self-test companion-migration)
set_tests_properties(companion_migration PROPERTIES TIMEOUT 90 FAIL_REGULAR_EXPRESSION "JUCE Assertion failure")
add_test(NAME manager_contract COMMAND CoreSampleManager --self-test)
add_test(NAME render_status COMMAND CoreSampleManager --self-test render-status)
set_tests_properties(render_status PROPERTIES TIMEOUT 90 FAIL_REGULAR_EXPRESSION "JUCE Assertion failure")
add_test(NAME start_tempo COMMAND CoreSampleManager --self-test start-tempo)
add_test(NAME tempo_detection COMMAND CoreSampleManager --self-test tempo)
set_tests_properties(tempo_detection PROPERTIES TIMEOUT 60 FAIL_REGULAR_EXPRESSION "JUCE Assertion failure")
add_test(NAME midi_settings COMMAND CoreSampleManager --self-test midi-settings)
add_test(NAME firmware_downloads COMMAND CoreSampleManager --self-test firmware)
set_tests_properties(firmware_downloads PROPERTIES TIMEOUT 60 FAIL_REGULAR_EXPRESSION "JUCE Assertion failure")
add_test(NAME firmware_catalog COMMAND ${CMAKE_COMMAND}
    -DCORE_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
    -DTEST_OUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/firmware-catalog-test
    -P ${CMAKE_CURRENT_SOURCE_DIR}/Tests/FirmwareCatalogTests.cmake)
set_tests_properties(midi_settings PROPERTIES TIMEOUT 60 FAIL_REGULAR_EXPRESSION "JUCE Assertion failure")
set_tests_properties(manager_contract PROPERTIES TIMEOUT 120 FAIL_REGULAR_EXPRESSION "JUCE Assertion failure")

if(APPLE)
  add_test(NAME native_midi COMMAND CoreSampleManager --midi-test)
  set_tests_properties(native_midi PROPERTIES TIMEOUT 30 FAIL_REGULAR_EXPRESSION "JUCE Assertion failure")
endif()
