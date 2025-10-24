cmake_minimum_required(VERSION 3.13)

# Set PICO_SDK_PATH if not already set
if(NOT DEFINED ENV{PICO_SDK_PATH})
    set(ENV{PICO_SDK_PATH} "${CMAKE_CURRENT_LIST_DIR}/pico-sdk")
endif()

# initialize the SDK based on PICO_SDK_PATH
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(input_detection_test C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# initialize the Pico SDK
pico_sdk_init()

add_executable(input_detection_test
    main.c
)

target_compile_definitions(input_detection_test PRIVATE
    # Pin definitions for ectocore
    GPIO_INPUTDETECT=16
    MCP_CV_AMEN=2
    MCP_CV_BREAK=5
    MCP_CV_SAMPLE=7
)

# Add pico_stdlib which includes required libraries
target_link_libraries(input_detection_test
    pico_stdlib
    hardware_spi
    hardware_gpio
)

# enable usb output, disable uart output
pico_enable_stdio_usb(input_detection_test 1)
pico_enable_stdio_uart(input_detection_test 0)

# create map/bin/hex file etc.
pico_add_extra_outputs(input_detection_test)
