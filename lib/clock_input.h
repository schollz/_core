// Copyright 2023 Zack Scholl.
//
// Author: Zack Scholl (zack.scholl@gmail.com)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// See http://creativecommons.org/licenses/MIT/ for more information.

#ifndef LIB_CLOCK_INPUT
#define LIB_CLOCK_INPUT

#include "filterexp.h"
#include "utils.h"

typedef struct ClockInput {
  uint8_t gpio;
  bool last_state;
  uint32_t last_time;
  int time_diff;
  FilterExp *filter;
  callback_int callback;
} ClockInput;

ClockInput *ClockInput_create(uint8_t gpio, callback_int callback) {
  ClockInput *ci = (ClockInput *)malloc(sizeof(ClockInput));
  ci->gpio = gpio;
  ci->last_state = 0;
  ci->last_time = time_us_32();
  ci->callback = callback;
  ci->filter = FilterExp_create(10);

  gpio_init(gpio);
  gpio_set_dir(gpio, GPIO_IN);
  gpio_pull_down(gpio);
  return ci;
}

void ClockInput_update(ClockInput *ci) {
  uint8_t clock_pin = 1 - gpio_get(ci->gpio);
  //   code to verify polarity
  if (clock_pin == 1 && ci->last_state == 0) {
    uint32_t now_time = time_us_32();
    int time_diff = FilterExp_update(ci->filter, now_time - ci->last_time);
    printf("[ClockInput] on after %d ms\n", time_diff / 1000);
    ci->callback(time_diff);
  }

  ci->last_state = clock_pin;
}

#endif
