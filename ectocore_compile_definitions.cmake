target_compile_definitions(${PROJECT_NAME} PRIVATE
    # hardware specific
    USE_AUDIO_I2S=1
    SDCARD_USE_CD=false
    SDCARD_CD_GPIO=21 # not used...

    # # ecto
    SDCARD_CMD_GPIO=25
    SDCARD_D0_GPIO=26 
    AUDIO_CLK_GPIO=21 # LCK=17
    AUDIO_DIN_GPIO=23
    INCLUDE_ECTOCORE=1
    INCLUDE_FILTER=1

    # utilize core1 for audio to avoid dropouts
    CORE1_PROCESS_I2S_CALLBACK=1 
    DO_OVERCLOCK=1
    
    # pin definitions
    MCP_KNOB_AMEN=0
    MCP_ATTEN_AMEN=1
    MCP_CV_AMEN=2
    MCP_KNOB_BREAK=3
    MCP_ATTEN_BREAK=4
    MCP_CV_BREAK=5
    MCP_KNOB_SAMPLE=6
    MCP_CV_SAMPLE=7
    GPIO_BTN_MODE=1
    GPIO_BTN_MULT=2
    GPIO_BTN_BANK=20
    GPIO_BTN_TAPTEMPO=3
    GPIO_LED_TAPTEMPO=4
    GPIO_INPUTDETECT=16
    GPIO_CLOCK_IN=17
    GPIO_CLOCK_OUT=19
    GPIO_TRIG_OUT=18
    GPIO_LED_MODE1=12
    GPIO_LED_MODE2=13
    GPIO_LED_MODE3=14
    GPIO_LED_MODE4=15


    # debug printing
    # PRINT_AUDIO_USAGE=1
    # PRINT_AUDIO_OVERLOADS=1
    # PRINT_AUDIO_CPU_USAGE=1
    # PRINT_MEMORY_USAGE=1
    # PRINT_SDCARD_TIMING=1

    # turn off gpio for leds
    LEDS_NO_GPIO=1

    # file variations
    FILE_VARIATIONS=2

    # basics 
    # INCLUDE_KEYBOARD=1
    # INCLUDE_RGBLED=1
    # INCLUDE_SINEBASS=1

    USBD_PID=0x1837
)

pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 1)