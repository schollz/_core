cmake_minimum_required (VERSION 3.13)

include(pico_sdk_import.cmake)
set (BINARY "pico_usb_kbd_lcd") 
set (PROJ "pico_usb_kbd_lcd")
project("pico_usb_kbd_lcd")

# As bizarre as it might seem, enabling these debug flags causes the 
#   program to crash when TinyUSB detects a device being attached. I have
#   no clue why, or even how it could be possible.
#set (CMAKE_C_FLAGS_RELEASE "-Wall -Wextra -Wconversion")
#set (CMAKE_C_FLAGS_DEBUG "-Wall -Wextra -Wconversion")

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file (GLOB usb_kbd_src CONFIGURE_DEPENDS "usb_kbd/src/*.c")
file (GLOB kbd_src CONFIGURE_DEPENDS "kbd/src/*.c")

add_executable(${BINARY}
    main.c
    ../../ssd1306.c
    ${i2c_lcd_src}
    ${usb_kbd_src}
    ${kbd_src}
)

target_include_directories (${BINARY} PUBLIC i2c_lcd/include)
target_include_directories (${BINARY} PUBLIC usb_kbd/include)
target_include_directories (${BINARY} PUBLIC kbd/include)
target_include_directories (${BINARY} PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries (${BINARY} PRIVATE pico_stdlib hardware_i2c tinyusb_host tinyusb_board)

pico_add_extra_outputs(${BINARY})
