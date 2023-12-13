# Set minimum required version of CMake
cmake_minimum_required(VERSION 3.12)

# Pull in PICO SDK (must be before project)
include(pico_sdk_import.cmake)

# We also need PICO EXTRAS
include(pico_extras_import.cmake)


# Set name of project (as PROJECT_NAME) and C/C++ standards
project(zeptocore C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Creates a pico-sdk subdirectory in our project for the libraries
pico_sdk_init()

# Tell CMake where to find the executable source file
add_executable(${PROJECT_NAME} 
    main.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/hw_config.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/pcg_basic.c
)


# Tell CMake where to find other source code
add_subdirectory(lib/my_pico_audio)
add_subdirectory(lib/my_pico_audio_i2s)
add_subdirectory(lib/sdio build)

# generate pio
pico_generate_pio_header(zeptocore ${CMAKE_CURRENT_LIST_DIR}/lib/WS2812.pio)
pico_generate_pio_header(zeptocore ${CMAKE_CURRENT_LIST_DIR}/lib/buttonmatrix3.pio)

# Create map/bin/hex/uf2 files
pico_add_extra_outputs(${PROJECT_NAME})

# Link to pico_stdlib (gpio, time, etc. functions)
target_link_libraries(${PROJECT_NAME} 
    pico_stdlib
    pico_multicore
    my_pico_audio_i2s
    FatFs_SPI
    hardware_clocks
    hardware_adc
    hardware_pio
    hardware_i2c
)

# target_compile_options(command_line PUBLIC -Wall -Wextra -Wno-unused-function -Wno-unused-parameter)
# target_compile_options(${PROJECT_NAME} PUBLIC -Wall -Wextra -Wno-unused-parameter)

# This program is useless without standard standard input and output.
# add_compile_definitions(USE_PRINTF USE_DBG_PRINTF)
add_compile_definitions(USE_PRINTF)

set_property(TARGET ${PROJECT_NAME} APPEND_STRING PROPERTY LINK_FLAGS "-Wl,--print-memory-usage")

# Enable usb output, disable uart output
pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 1)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    # hardware specific
    USE_AUDIO_I2S=1
    SDCARD_USE_CD=false
    SDCARD_CD_GPIO=21 # not used...
    SDCARD_CMD_GPIO=2
    SDCARD_D0_GPIO=3 # CLK=17,D1=20,D2=21,D3=22
    AUDIO_CLK_GPIO=16 # LCK=17
    AUDIO_DIN_GPIO=18
    I2C_SDA_PIN=20
    I2C_SCL_PIN=21
    LED_1_GPIO=0
    LED_2_GPIO=0
    LED_3_GPIO=19
    LED_4_GPIO=19
    CLOCK_INPUT_GPIO=22
    BTN_ROW_START=7
    BTN_COL_START=12

# v15
# SDCARD_CD_GPIO=21 # not used...
# SDCARD_CMD_GPIO=10
# SDCARD_D0_GPIO=11 # CLK=17,D1=20,D2=21,D3=22
# AUDIO_CLK_GPIO=16 # LCK=17
# AUDIO_DIN_GPIO=18
# I2C_SDA_PIN=20
# I2C_SCL_PIN=21
# LED_1_GPIO=15
# LED_2_GPIO=19
# LED_3_GPIO=29
# LED_4_GPIO=24
# CLOCK_INPUT_GPIO=22
# BTN_ROW_START=0
# BTN_COL_START=5


    # utilize core1 for audio to avoid dropouts
    CORE1_PROCESS_I2S_CALLBACK=1 

    # debug printing
    PRINT_AUDIO_USAGE=1
    # PRINT_AUDIO_CPU_USAGE=1
    # PRINT_SDCARD_TIMING=1

    # turn off gpio for leds
    LEDS_NO_GPIO=1

    # file variations
    FILE_VARIATIONS=3

    # includes for different versions
    # INCLUDE_RGBLED=1
    INCLUDE_FILTER=1
    # INCLUDE_TRANSFER=1 
    # INCLUDE_BASS=1
)

pico_add_extra_outputs(${PROJECT_NAME})
