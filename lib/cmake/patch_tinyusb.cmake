# Match the firmware's TinyUSB endpoint mutex timeout policy on every build path.
# Run after pico_sdk_init(), or directly with -DPICO_SDK_PATH=... -P this file.
if(PICO_TINYUSB_PATH)
    set(CORE_TINYUSB_SOURCE "${PICO_TINYUSB_PATH}/src/tusb.c")
else()
    set(CORE_TINYUSB_SOURCE "${PICO_SDK_PATH}/lib/tinyusb/src/tusb.c")
endif()
if(NOT EXISTS "${CORE_TINYUSB_SOURCE}")
    message(FATAL_ERROR "TinyUSB source missing: ${CORE_TINYUSB_SOURCE}. Initialize the Pico SDK submodules before building.")
endif()
file(READ "${CORE_TINYUSB_SOURCE}" CORE_TINYUSB_ORIGINAL)
# Both SDK 2.2.0 and 2.3.1 use these two endpoint lock calls. Refuse to
# silently skip the workaround when an upstream change needs review.
string(REGEX MATCHALL "osal_mutex_lock\\(mutex, OSAL_TIMEOUT_(WAIT_FOREVER|NORMAL)\\)"
       CORE_TINYUSB_LOCKS "${CORE_TINYUSB_ORIGINAL}")
list(LENGTH CORE_TINYUSB_LOCKS CORE_TINYUSB_LOCK_COUNT)
if(NOT CORE_TINYUSB_LOCK_COUNT EQUAL 2)
    message(FATAL_ERROR "Unexpected TinyUSB endpoint lock layout in ${CORE_TINYUSB_SOURCE}; review the timeout patch before building.")
endif()
string(REPLACE "osal_mutex_lock(mutex, OSAL_TIMEOUT_WAIT_FOREVER)" "osal_mutex_lock(mutex, OSAL_TIMEOUT_NORMAL)"
       CORE_TINYUSB_PATCHED "${CORE_TINYUSB_ORIGINAL}")
# Do not touch the timestamp on subsequent configurations: avoid needless rebuilds.
if(NOT CORE_TINYUSB_PATCHED STREQUAL CORE_TINYUSB_ORIGINAL)
    file(WRITE "${CORE_TINYUSB_SOURCE}" "${CORE_TINYUSB_PATCHED}")
    message(STATUS "Patched TinyUSB endpoint mutex timeouts to OSAL_TIMEOUT_NORMAL")
endif()
unset(CORE_TINYUSB_SOURCE)
unset(CORE_TINYUSB_ORIGINAL)
unset(CORE_TINYUSB_PATCHED)
unset(CORE_TINYUSB_LOCKS)
unset(CORE_TINYUSB_LOCK_COUNT)
