# Match the firmware's TinyUSB endpoint mutex timeout policy on every build path.
# Run after pico_sdk_init(), or directly with -DPICO_SDK_PATH=... -P this file.
set(CORE_TINYUSB_SOURCE "${PICO_SDK_PATH}/lib/tinyusb/src/tusb.c")
if(NOT EXISTS "${CORE_TINYUSB_SOURCE}")
    message(FATAL_ERROR "TinyUSB source missing: ${CORE_TINYUSB_SOURCE}. Initialize the Pico SDK submodules before building.")
endif()
file(READ "${CORE_TINYUSB_SOURCE}" CORE_TINYUSB_ORIGINAL)
string(REPLACE "OSAL_TIMEOUT_WAIT_FOREVER" "OSAL_TIMEOUT_NORMAL"
       CORE_TINYUSB_PATCHED "${CORE_TINYUSB_ORIGINAL}")
# Do not touch the timestamp on subsequent configurations: avoid needless rebuilds.
if(NOT CORE_TINYUSB_PATCHED STREQUAL CORE_TINYUSB_ORIGINAL)
    file(WRITE "${CORE_TINYUSB_SOURCE}" "${CORE_TINYUSB_PATCHED}")
    message(STATUS "Patched TinyUSB endpoint mutex timeouts to OSAL_TIMEOUT_NORMAL")
endif()
unset(CORE_TINYUSB_SOURCE)
unset(CORE_TINYUSB_ORIGINAL)
unset(CORE_TINYUSB_PATCHED)
