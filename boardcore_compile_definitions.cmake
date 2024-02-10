target_compile_definitions(${PROJECT_NAME} PRIVATE
    # hardware specific
    USE_AUDIO_I2S=1
    SDCARD_USE_CD=false
    SDCARD_CD_GPIO=21 # not used...

    # utilize core1 for audio to avoid dropouts
    SDCARD_CMD_GPIO=11
    SDCARD_D0_GPIO=12 
    AUDIO_CLK_GPIO=16 
    AUDIO_DIN_GPIO=18
    INCLUDE_FILTER=1
    INCLUDE_BOARDCORE=1
    CORE1_PROCESS_I2S_CALLBACK=1 
    DO_OVERCLOCK=1

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
)
