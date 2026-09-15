# Set minimum required version of CMake
cmake_minimum_required(VERSION 3.12)

# Pull in PICO SDK (must be before project)
include(pico_sdk_import.cmake)

# We also need PICO EXTRAS
include(pico_extras_import.cmake)


# Set name of project (as PROJECT_NAME) and C/C++ standards
project(_core C CXX ASM)
option(SEEK_DIAGNOSTICS "Collect bounded SWD seek/audio diagnostics" OFF)
option(SEEK_TIMING_WITNESS "Independent callback timing for paired observer tests" OFF)
option(SEEK_TEST_CONTROLS "Enable reserved MIDI CCs for repeatable hardware tests" OFF)
option(SEEK_MAP_ATTACH "Attach validated seek maps (disable only for comparison tests)" ON)
option(SEEK_TEST_FRESH_INDEX "Use a separate firmware-owned test index directory" OFF)
option(SEEK_TEST_FRAGMENT_BENCH "Create a reserved synthetic fixture and benchmark at boot" OFF)
set(SEEK_TEST_AUDIO_FIXTURE "0" CACHE STRING "Test audio fixture: 0 off; 1/2 add/remove .2; 3/4 add/remove .3")
if(NOT SEEK_TEST_AUDIO_FIXTURE MATCHES "^[01234]$")
    message(FATAL_ERROR "SEEK_TEST_AUDIO_FIXTURE must be 0..4")
endif()
if(SEEK_TEST_AUDIO_FIXTURE AND (NOT SEEK_TEST_CONTROLS OR NOT SEEK_DIAGNOSTICS))
    message(FATAL_ERROR "SEEK_TEST_AUDIO_FIXTURE requires diagnostics and test controls")
endif()
if(SEEK_TEST_FRAGMENT_BENCH AND (NOT SEEK_TEST_CONTROLS OR NOT SEEK_DIAGNOSTICS))
    message(FATAL_ERROR "SEEK_TEST_FRAGMENT_BENCH requires diagnostics and test controls")
endif()
if(SEEK_TEST_FRESH_INDEX AND NOT SEEK_TEST_CONTROLS)
    message(FATAL_ERROR "SEEK_TEST_FRESH_INDEX requires SEEK_TEST_CONTROLS")
endif()
option(CORE_NO_OVERCLOCK "Use the firmware's 125 MHz non-overclocked clock path" OFF)
set(CORE_COMPILE_DEFINITIONS "${CMAKE_CURRENT_LIST_DIR}/target_compile_definitions.cmake"
    CACHE FILEPATH "Device compile definitions for an isolated build")
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Creates a pico-sdk subdirectory in our project for the libraries
pico_sdk_init()

# error if there is a warning
# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror -Wall")

# Tell CMake where to find the executable source file
add_executable(${PROJECT_NAME} 
    main.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/seek_diagnostics.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/audio_seek_map.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/seek_fragment_benchmark.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/seek_audio_fixture.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/audio_media_owner.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/hw_config.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/pcg_basic.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/usb_descriptors.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/ssd1306.c
)


# Tell CMake where to find other source code
add_subdirectory(lib/my_pico_audio)
add_subdirectory(lib/my_pico_audio_i2s)
add_subdirectory(lib/mcp23017)
add_subdirectory(lib/sdio build)

# generate pio
pico_generate_pio_header(_core ${CMAKE_CURRENT_LIST_DIR}/lib/WS2812.pio)
pico_generate_pio_header(_core ${CMAKE_CURRENT_LIST_DIR}/lib/buttonmatrix3.pio)
pico_generate_pio_header(_core ${CMAKE_CURRENT_LIST_DIR}/lib/onewiremidi.pio)
pico_generate_pio_header(_core ${CMAKE_CURRENT_LIST_DIR}/lib/uart_rx.pio)

# Create map/bin/hex/uf2 files
pico_add_extra_outputs(${PROJECT_NAME})

# Link to pico_stdlib (gpio, time, etc. functions)
target_link_libraries(${PROJECT_NAME} 
    pico_stdlib
    pico_multicore
    my_pico_audio_i2s
    FatFs_SPI
    mcp23017_lib
    hardware_clocks
    hardware_flash
    hardware_adc
    hardware_pio
    hardware_i2c
    hardware_sync
)

# target_compile_options(command_line PUBLIC -Wall -Wextra -Wno-unused-function -Wno-unused-parameter)
# target_compile_options(${PROJECT_NAME} PUBLIC -Wall -Wextra -Wno-unused-parameter)

# This program is useless without standard standard input and output.
# add_compile_definitions(USE_PRINTF USE_DBG_PRINTF)
add_compile_definitions(USE_PRINTF)

set_property(TARGET ${PROJECT_NAME} APPEND_STRING PROPERTY LINK_FLAGS "-Wl,--print-memory-usage")
target_compile_options(${PROJECT_NAME} PRIVATE
    # -Wall
    # -Wextra
    -O2
)

include(${CORE_COMPILE_DEFINITIONS})
target_compile_definitions(${PROJECT_NAME} PRIVATE SEEK_MAP_MODULE=1 PICO_MALLOC_PANIC=0
    SEEK_MAP_ATTACH=$<BOOL:${SEEK_MAP_ATTACH}> SEEK_TEST_FRESH_INDEX=$<BOOL:${SEEK_TEST_FRESH_INDEX}>
    SEEK_TEST_FRAGMENT_BENCH=$<BOOL:${SEEK_TEST_FRAGMENT_BENCH}>
    SEEK_TEST_AUDIO_FIXTURE=${SEEK_TEST_AUDIO_FIXTURE})
if(SEEK_TEST_FRESH_INDEX)
    target_compile_definitions(${PROJECT_NAME} PRIVATE SEEK_MAP_DIRECTORY=".core_seek_test")
endif()

if(CORE_NO_OVERCLOCK)
    get_target_property(CORE_DEVICE_DEFINITIONS ${PROJECT_NAME} COMPILE_DEFINITIONS)
    list(FILTER CORE_DEVICE_DEFINITIONS EXCLUDE REGEX "^DO_OVERCLOCK(=|$)")
    set_property(TARGET ${PROJECT_NAME} PROPERTY COMPILE_DEFINITIONS ${CORE_DEVICE_DEFINITIONS})
endif()

if(SEEK_DIAGNOSTICS)
    # Regenerate the identity whenever firmware sources or configuration change.
    file(GLOB_RECURSE SEEK_ID_INPUTS CONFIGURE_DEPENDS
         "${CMAKE_CURRENT_LIST_DIR}/lib/*.h" "${CMAKE_CURRENT_LIST_DIR}/lib/*.c"
         "${CMAKE_CURRENT_LIST_DIR}/lib/*.cpp" "${CMAKE_CURRENT_LIST_DIR}/lib/*.pio"
         "${CMAKE_CURRENT_LIST_DIR}/lib/CMakeLists.txt")
    list(APPEND SEEK_ID_INPUTS "${CMAKE_CURRENT_LIST_DIR}/main.c"
         "${CMAKE_CURRENT_LIST_DIR}/CMakeLists.txt" "${CORE_COMPILE_DEFINITIONS}")
    set(SEEK_ID_TEXT "${CMAKE_C_COMPILER_ID};${CMAKE_C_COMPILER_VERSION};${CMAKE_BUILD_TYPE};${CMAKE_C_FLAGS};${SEEK_TIMING_WITNESS};${SEEK_TEST_CONTROLS};${CORE_NO_OVERCLOCK};${SEEK_MAP_ATTACH};${SEEK_TEST_FRESH_INDEX};${SEEK_TEST_FRAGMENT_BENCH};${SEEK_TEST_AUDIO_FIXTURE}")
    foreach(SEEK_ID_INPUT IN LISTS SEEK_ID_INPUTS)
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${SEEK_ID_INPUT}")
        file(SHA256 "${SEEK_ID_INPUT}" SEEK_FILE_HASH)
        string(APPEND SEEK_ID_TEXT "${SEEK_ID_INPUT}:${SEEK_FILE_HASH};")
    endforeach()
    string(SHA256 SEEK_ID_HASH "${SEEK_ID_TEXT}")
    set(SEEK_ID_WORDS "")
    foreach(SEEK_ID_INDEX RANGE 0 7)
        math(EXPR SEEK_ID_OFFSET "${SEEK_ID_INDEX} * 8")
        string(SUBSTRING "${SEEK_ID_HASH}" ${SEEK_ID_OFFSET} 8 SEEK_ID_WORD)
        string(APPEND SEEK_ID_WORDS "0x${SEEK_ID_WORD}u,")
    endforeach()
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/seek_build_identity.h"
         "#define ZD_BUILD_WORDS ${SEEK_ID_WORDS}\n")
    target_compile_definitions(${PROJECT_NAME} PRIVATE SEEK_DIAGNOSTICS=1)
    target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
    target_compile_options(${PROJECT_NAME} PRIVATE -fstack-usage)
endif()

if(SEEK_TIMING_WITNESS)
    target_compile_definitions(${PROJECT_NAME} PRIVATE SEEK_TIMING_WITNESS=1)
endif()

if(SEEK_TEST_CONTROLS)
    target_compile_definitions(${PROJECT_NAME} PRIVATE SEEK_TEST_CONTROLS=1)
endif()

pico_add_extra_outputs(${PROJECT_NAME})
