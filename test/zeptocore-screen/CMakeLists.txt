cmake_minimum_required(VERSION 3.13)
include(../../lib/cmake/pico_sdk_import.cmake)
project(zeptocore_screen C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

add_executable(zeptocore-screen main.c ../../lib/sharp_memory.c)
target_include_directories(zeptocore-screen PRIVATE ../../lib)
target_link_libraries(zeptocore-screen pico_stdlib hardware_spi hardware_dma)
target_compile_options(zeptocore-screen PRIVATE -Wall -Wextra -Werror)
target_compile_definitions(zeptocore-screen PRIVATE
    PICO_XOSC_STARTUP_DELAY_MULTIPLIER=4
    USBD_PRODUCT="zeptocore-screen"
    CFG_TUSB_CONFIG_FILE="${PICO_SDK_PATH}/src/rp2_common/pico_stdio_usb/include/tusb_config.h"
)
pico_enable_stdio_usb(zeptocore-screen 1)
pico_enable_stdio_uart(zeptocore-screen 0)
pico_set_program_name(zeptocore-screen "zeptocore-screen")
pico_set_program_version(zeptocore-screen "0.1.0")
pico_add_extra_outputs(zeptocore-screen)
