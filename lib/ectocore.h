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

/* spec
Amen is for slice based effects (how many slices and how often it jumps)
Break would be the wacky effects (filtering, reversing, etc.)
Sample selects currently playing sample from the current bank
*/

#include "mcp3208.h"

void go_retrigger_2key(uint8_t key1, uint8_t key2) {
  debounce_quantize = 0;
  retrig_first = true;
  retrig_beat_num = random_integer_in_range(8, 24);
  retrig_timer_reset =
      96 * random_integer_in_range(1, 6) / random_integer_in_range(2, 12);
  float total_time = (float)(retrig_beat_num * retrig_timer_reset * 60) /
                     (float)(96 * sf->bpm_tempo);
  if (total_time > 5.0f) {
    total_time = total_time / 2;
    retrig_timer_reset = retrig_timer_reset / 2;
  }
  if (total_time > 5.0f) {
    total_time = total_time / 2;
    retrig_beat_num = retrig_beat_num / 2;
    if (retrig_beat_num == 0) {
      retrig_beat_num = 1;
    }
  }
  if (total_time < 0.5f) {
    total_time = total_time * 2;
    retrig_beat_num = retrig_beat_num * 2;
    if (retrig_beat_num == 0) {
      retrig_beat_num = 1;
    }
  }
  if (total_time < 0.5f) {
    total_time = total_time * 2;
    retrig_beat_num = retrig_beat_num * 2;
    if (retrig_beat_num == 0) {
      retrig_beat_num = 1;
    }
  }
  retrig_vol_step = 1.0 / ((float)retrig_beat_num);
  // printf("retrig_beat_num=%d,retrig_timer_reset=%d,total_time=%2.3fs\n",
  //        retrig_beat_num, retrig_timer_reset, total_time);
  retrig_ready = true;
}

void input_handling() {
  // flash bad signs
  while (!fil_is_open) {
    printf("waiting to start\n");
    sleep_ms(10);
  }

  gpio_init(GPIO_TAPTEMPO);
  gpio_set_dir(GPIO_TAPTEMPO, GPIO_IN);
  gpio_pull_up(GPIO_TAPTEMPO);
  gpio_init(GPIO_TAPTEMPO_LED);
  gpio_set_dir(GPIO_TAPTEMPO_LED, GPIO_OUT);
  gpio_put(GPIO_TAPTEMPO_LED, 1);

  MCP3208 *mcp3208 = MCP3208_malloc(spi1, 9, 10, 8, 11);

  bool btn_taptempo_on = false;
  sf->vol = VOLUME_STEPS;
  sf->fx_active[FX_SATURATE] = true;
  Saturation_setActive(saturation, sf->fx_active[FX_SATURATE]);
  while (1) {
    uint16_t val;

    // check for input
    int char_input = getchar_timeout_us(10);
    if (char_input >= 0) {
      if (char_input == 118) {
        printf("version=v1.4.2\n");
      }
    }

    if (MessageSync_hasMessage(messagesync)) {
      MessageSync_print(messagesync);
      MessageSync_clear(messagesync);
    }

#ifdef PRINT_SDCARD_TIMING
    // random stuff
    if (random_integer_in_range(1, 20000) < 10) {
      // printf("random retrig\n");
      key_do_jump(random_integer_in_range(0, 15));
    } else if (random_integer_in_range(1, 20000) < 5) {
      // printf("random retrigger\n");
      go_retrigger_2key(1, 1);
    }
#endif

    if (gpio_get(GPIO_TAPTEMPO) == 0 && !btn_taptempo_on) {
      btn_taptempo_on = true;
      gpio_put(GPIO_TAPTEMPO_LED, 0);
      val = TapTempo_tap(taptempo);
      if (val > 0) {
        printf("[ectocore] tap bpm -> %d\n", val);
        sf->bpm_tempo = val;
      }
    } else if (gpio_get(GPIO_TAPTEMPO) == 1 && btn_taptempo_on) {
      btn_taptempo_on = false;
      gpio_put(GPIO_TAPTEMPO_LED, 1);
    }

    val = MCP3208_read(mcp3208, KNOB_SAMPLE, false);
    val = (val * (banks[sel_bank_next]->num_samples)) / 1024;
    if (val != sel_sample_cur) {
      sel_sample_next = val;
      fil_current_change = true;
      printf("[ectocore] switch sample %d\n", val);
    }
    sleep_ms(1);
  }
}