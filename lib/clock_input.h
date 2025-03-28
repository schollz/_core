// Copyright 2023-2025 Zack Scholl, GPLv3.0

#ifndef LIB_CLOCK_INPUT
#define LIB_CLOCK_INPUT

#include "filterexp.h"
#include "utils.h"

typedef struct ClockInput {
  uint8_t gpio;
  bool last_state;
  uint32_t last_time;
  uint32_t last_diff;
  FilterExpUint32 *filter;
  callback_int callback_up;
  callback_int callback_down;
  callback_void callback_start;
} ClockInput;

void ClockInput_destroy(ClockInput *ci) {
  // make sure ci is initialized
  if (ci != NULL) {
    // free the filter
    FilterExpUint32_free(ci->filter);
    // free the ci struct
    free(ci);
  }
}

ClockInput *ClockInput_create(uint8_t gpio, callback_int callback_up,
                              callback_int callback_down,
                              callback_void callback_start) {
  ClockInput *ci = (ClockInput *)malloc(sizeof(ClockInput));
  ci->gpio = gpio;
  ci->last_state = 0;
  ci->last_time = time_us_32();
  ci->callback_up = callback_up;
  ci->callback_down = callback_down;
  ci->callback_start = callback_start;
  ci->filter = FilterExpUint32_create(180);
  ci->last_diff = 0;

  // initialize filter to reasonable level (120 bpm)
  for (uint8_t i = 0; i < 100; i++) {
    FilterExpUint32_update(ci->filter, 250000);
  }

  gpio_init(gpio);
  gpio_set_dir(gpio, GPIO_IN);
#ifdef INCLUDE_ECTOCORE
  gpio_pull_up(gpio);
#else
  gpio_pull_down(gpio);
#endif
  return ci;
}

int32_t ClockInput_timeSinceLast(ClockInput *ci) {
  return time_us_32() - ci->last_time;
}

void ClockInput_update_raw(ClockInput *ci, uint8_t clock_pin) {
  if (clock_pin == 1 && ci->last_state == 0) {
    uint32_t now_time = time_us_32();
    if (now_time > ci->last_time) {
      uint32_t time_diff = now_time - ci->last_time;
      // printf("[clock_input] time diff: %d\n", time_diff);
      if (time_diff < 2 * ci->last_diff && time_diff > 1000) {
        if (ci->callback_up != NULL) {
          ci->callback_up(FilterExpUint32_update(ci->filter, time_diff));
        }
      } else {
        if (ci->callback_start != NULL) {
          ci->callback_start();
        }
      }
      ci->last_diff = time_diff;
    }
    ci->last_time = now_time;
  } else if (clock_pin == 0 && ci->last_state == 1) {
    if (ci->callback_down != NULL) {
      ci->callback_down(time_us_32() - ci->last_time);
    }
  }
  ci->last_state = clock_pin;
}

void ClockInput_update(ClockInput *ci) {
  uint8_t clock_pin = 1 - gpio_get(ci->gpio);
  ClockInput_update_raw(ci, clock_pin);
}

#endif
