# Set minimum required version of CMake
cmake_minimum_required(VERSION 3.12)

# Pull in PICO SDK (must be before project)
include(pico_sdk_import.cmake)

# We also need PICO EXTRAS
include(pico_extras_import.cmake)


# Set name of project (as PROJECT_NAME) and C/C++ standards
project(_core C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Creates a pico-sdk subdirectory in our project for the libraries
pico_sdk_init()

# error if there is a warning
# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror -Wall")

# Tell CMake where to find the executable source file
add_executable(${PROJECT_NAME} 
    main.c
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
    -Wall
    -Wextra
    -O3
)

include(target_compile_definitions.cmake)

pico_add_extra_outputs(${PROJECT_NAME})
