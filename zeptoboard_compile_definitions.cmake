target_compile_definitions(${PROJECT_NAME} PRIVATE
    # hardware specific
    USE_AUDIO_I2S=1
    SDCARD_USE_CD=false
    SDCARD_CD_GPIO=21 # not used...
    PICO_XOSC_STARTUP_DELAY_MULTIPLIER=4

    # utilize core1 for audio to avoid dropouts
    SAMPLES_PER_BUFFER=441
    SDCARD_CMD_GPIO=11
    SDCARD_D0_GPIO=12 
    AUDIO_CLK_GPIO=16 
    AUDIO_DIN_GPIO=18
    INCLUDE_FILTER=1
    INCLUDE_BOARDCORE=1
    CORE1_PROCESS_I2S_CALLBACK=1 
    DO_OVERCLOCK=1
    INCLUDE_MIDI=1
    # INCLUDE_SSD1306=1
    
    # debug printing
    # PRINT_AUDIO_USAGE=1
    # PRINT_AUDIO_OVERLOADS=1
    # PRINT_AUDIO_CPU_USAGE=1
    # PRINT_MEMORY_USAGE=1
    # PRINT_SDCARD_TIMING=1

    # ARCADE DEFINITIONS
    MCP23017_ADDR1=0x20
    MCP23017_ADDR2=0x21
    ADS7830_ADDR=0x48

    # turn off gpio for leds
    LEDS_NO_GPIO=1

    # file variations
    FILE_VARIATIONS=2

    # basics 
    # INCLUDE_KEYBOARD=1
    # INCLUDE_RGBLED=1
    # INCLUDE_SINEBASS=1

    USBD_PID=0x1838
)

pico_enable_stdio_usb(${PROJECT_NAME} 1)
# uncomment these lines to include midi
target_link_libraries(${PROJECT_NAME} 
    tinyusb_device
    tinyusb_board
)
pico_enable_stdio_usb(${PROJECT_NAME} 0)
pico_enable_stdio_uart(${PROJECT_NAME} 1)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR})

# # uncomment these lines to have normal USB
# pico_enable_stdio_usb(${PROJECT_NAME} 1)
# pico_enable_stdio_uart(${PROJECT_NAME} 1)
