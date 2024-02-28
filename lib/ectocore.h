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
- [ ] knob_break: wacky effects
- [ ] knob_amen: slice based effects
- [x] knob_sample: switch sample
- [ ] knob_break_atten:
- [ ] knob_amen_atten:
- [ ] btn_bank: switch banks
- [ ] btn_mode: changes trigger output mode
- [x] btn_mult: sets clock division
- [x] btn_bank + knob_sample: switch bank
- [ ] btn_tap: sets tempo (3+ presses)
- [x] btn_tap + knob_break: volume / loss / overdrive
- [ ] btn_tap + btn_bank: mute
- [ ] amen_cv:
- [ ] break_cv:
- [ ] sample_cv:
- [ ] clk_in:
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

  gpio_init(GPIO_LED_TAPTEMPO);
  gpio_set_dir(GPIO_LED_TAPTEMPO, GPIO_OUT);
  gpio_put(GPIO_LED_TAPTEMPO, 1);
  gpio_init(GPIO_TRIG_OUT);
  gpio_set_dir(GPIO_TRIG_OUT, GPIO_OUT);
  gpio_init(GPIO_INPUTDETECT);
  gpio_set_dir(GPIO_INPUTDETECT, GPIO_OUT);
  gpio_init(GPIO_CLOCK_OUT);
  gpio_set_dir(GPIO_CLOCK_OUT, GPIO_OUT);

  MCP3208 *mcp3208 = MCP3208_malloc(spi1, 9, 10, 8, 11);

  bool btn_taptempo_on = false;
  sf->vol = VOLUME_STEPS;
  sf->fx_active[FX_SATURATE] = true;
  sf->pitch_val_index = PITCH_VAL_MID;
  uint8_t debounce_trig = 0;
  Saturation_setActive(saturation, sf->fx_active[FX_SATURATE]);

  uint16_t debounce_input_detection = 0;
  uint8_t magic_signal[9] = {0, 1, 1, 1, 0, 1, 0, 1, 1};

  // update the knobs
#define KNOB_NUM 5
  uint8_t knob_gpio[KNOB_NUM] = {
      MCP_KNOB_BREAK, MCP_ATTEN_BREAK, MCP_KNOB_AMEN,
      MCP_ATTEN_AMEN, MCP_KNOB_SAMPLE,
  };
  KnobChange *knob_change[KNOB_NUM];
  for (uint8_t i = 0; i < KNOB_NUM; i++) {
    knob_change[i] = KnobChange_malloc();
  }

#define BUTTON_NUM 4
  const uint8_t gpio_btns[BUTTON_NUM] = {
      GPIO_BTN_MODE,
      GPIO_BTN_MULT,
      GPIO_BTN_BANK,
      GPIO_BTN_TAPTEMPO,
  };
  ButtonChange *button_change[BUTTON_NUM];
  for (uint8_t i = 0; i < BUTTON_NUM; i++) {
    gpio_init(gpio_btns[i]);
    gpio_set_dir(gpio_btns[i], GPIO_IN);
    gpio_pull_up(gpio_btns[i]);
    button_change[i] = ButtonChange_malloc();
  }

  while (1) {
    int16_t val;

    if (debounce_input_detection > 0) {
      debounce_input_detection--;
    } else {
      // input detection
      int16_t val;
      uint8_t length_signal = 9;
      uint8_t response_signal[3][9] = {
          {0, 0, 0, 0, 0, 0, 0, 0, 0},
          {0, 0, 0, 0, 0, 0, 0, 0, 0},
          {0, 0, 0, 0, 0, 0, 0, 0, 0},
      };
      uint8_t cv_signals[3] = {MCP_CV_AMEN, MCP_CV_BREAK, MCP_CV_SAMPLE};

      for (uint8_t i = 0; i < length_signal; i++) {
        gpio_put(GPIO_INPUTDETECT, magic_signal[i]);
        sleep_us(2);
        for (uint8_t j = 0; j < 3; j++) {
          val = MCP3208_read(mcp3208, cv_signals[j], false);
          if (val > 700) {
            response_signal[j][i] = 1;
          }
        }
      }
      bool is_signal[3] = {true, true, true};
      for (uint8_t j = 0; j < 3; j++) {
        for (uint8_t i = 0; i < length_signal; i++) {
          if (response_signal[j][i] != magic_signal[i]) {
            is_signal[j] = false;
            break;
          }
        }
      }
      for (uint8_t j = 0; j < 3; j++) {
        if (j == 0) {
          cv_amen_plugged = is_signal[j];
        } else if (j == 1) {
          cv_break_plugged = is_signal[j];
        } else if (j == 2) {
          cv_sample_plugged = is_signal[j];
        }
      }
      debounce_input_detection = 100;
    }

    if (clock_out_ready) {
      clock_out_ready = false;
      uint16_t j = beat_current;
      while (j > banks[sel_bank_cur]
                     ->sample[sel_sample_cur]
                     .snd[sel_variation]
                     ->slice_num) {
        j -= banks[sel_bank_cur]
                 ->sample[sel_sample_cur]
                 .snd[sel_variation]
                 ->slice_num;
      }
      if (banks[sel_bank_cur]
                  ->sample[sel_sample_cur]
                  .snd[sel_variation]
                  ->slice_type[j] == 1 ||
          banks[sel_bank_cur]
                  ->sample[sel_sample_cur]
                  .snd[sel_variation]
                  ->slice_type[j] == 3) {
        gpio_put(GPIO_TRIG_OUT, 1);
        debounce_trig = 100;
      }
    } else if (debounce_trig > 0) {
      debounce_trig--;
      if (debounce_trig == 0) {
        gpio_put(GPIO_TRIG_OUT, 0);
      }
    }
    // check for input
    int char_input = getchar_timeout_us(10);
    if (char_input >= 0) {
      if (char_input == 118) {
        printf("version=v1.6.3\n");
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

    if (gpio_get(GPIO_BTN_TAPTEMPO) == 0 && !btn_taptempo_on) {
      btn_taptempo_on = true;
      gpio_put(GPIO_LED_TAPTEMPO, 0);
      val = TapTempo_tap(taptempo);
      if (val > 0) {
        printf("[ectocore] tap bpm -> %d\n", val);
        sf->bpm_tempo = val;
      }
    } else if (gpio_get(GPIO_BTN_TAPTEMPO) == 1 && btn_taptempo_on) {
      btn_taptempo_on = false;
      gpio_put(GPIO_LED_TAPTEMPO, 1);
    }

    for (uint8_t i = 0; i < KNOB_NUM; i++) {
      val = KnobChange_update(knob_change[i],
                              MCP3208_read(mcp3208, knob_gpio[i], false));
      if (val < 0) {
        continue;
      }
      if (knob_gpio[i] == MCP_KNOB_SAMPLE) {
        if (gpio_get(GPIO_BTN_BANK) == 0) {
          // bank selection
          printf("[ectocore] switch bank %d\n", val);
          val = (val * banks_with_samples_num) / 1024;
          uint8_t bank_num = 0;
          for (uint8_t j = 0; j < banks_with_samples_num; j++) {
            if (banks[j]->num_samples > 0) {
              if (bank_num == val) {
                sel_bank_next = j;
                printf("[ectocore] switch bank %d\n", val);
                break;
              }
              bank_num++;
            }
          }
        } else {
          // sample selection
          printf("[ectocore] switch sample %d\n", val);
          val = (val * banks[sel_bank_next]->num_samples) / 1024;
          if (val != sel_sample_cur) {
            sel_sample_next = val;
            fil_current_change = true;
            printf("[ectocore] switch sample %d\n", val);
          }
        }
      } else if (knob_gpio[i] == MCP_KNOB_BREAK) {
        printf("[ectocore] knob_break %d\n", val);
        if (gpio_get(GPIO_BTN_TAPTEMPO) == 0) {
          // change volume
          if (val < 412) {
            sf->vol = val * VOLUME_STEPS / 412;
            sf->fx_active[FX_FUZZ] = 0;
            sf->fx_active[FX_SATURATE] = 0;
          } else if (val > 700 && val <= 850) {
            sf->vol = VOLUME_STEPS;
            sf->fx_active[FX_FUZZ] = 0;
            sf->fx_active[FX_SATURATE] = 1;
          } else if (val > 850) {
            sf->vol = VOLUME_STEPS;
            sf->fx_active[FX_SATURATE] = 0;
            sf->fx_active[FX_FUZZ] = 1;
            sf->fx_param[FX_FUZZ][0] = (val - 850) * 255 / (1024 - 850);
          } else {
            sf->vol = VOLUME_STEPS;
            sf->fx_active[FX_FUZZ] = 0;
            sf->fx_active[FX_SATURATE] = 0;
          }
        }
      }
    }

    // button selection
    for (uint8_t i = 0; i < BUTTON_NUM; i++) {
      val = ButtonChange_update(button_change[i], gpio_get(gpio_btns[i]));
      if (val < 0) {
        continue;
      }
      val = 1 - val;
      if (gpio_btns[i] == GPIO_BTN_MODE) {
        printf("[ectocore] btn_mode %d\n", val);
      } else if (gpio_btns[i] == GPIO_BTN_BANK) {
        printf("[ectocore] btn_bank %d\n", val);
      } else if (gpio_btns[i] == GPIO_BTN_MULT) {
        printf("[ectocore] btn_mult %d\n", val);
        if (val == 1) {
          if (ectocore_clock_selected_division <
              ECTOCORE_CLOCK_NUM_DIVISIONS - 1) {
            ectocore_clock_selected_division++;
          } else {
            ectocore_clock_selected_division = 0;
          }
        }
      }
    }

    sleep_ms(1);
  }
}