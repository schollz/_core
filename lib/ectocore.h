// Copyright 2023-2024 Zack Scholl.
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
- [x] knob_break -> 0: mute/unmute playback
- [ ] knob_amen: slice based effects
- [x] knob_amen -> 0: stop/start playback
- [x] knob_sample: switch sample
- [ ] knob_break_atten:
- [ ] knob_amen_atten:
- [ ] btn_bank: switch banks
- [ ] btn_mode: changes trigger output mode
- [x] btn_mult: sets clock division
- [x] btn_bank + knob_sample: switch bank
- [x] btn_tap: sets tempo (3+ presses)
- [x] btn_tap + knob_break: volume / loss / overdrive
- [ ] btn_tap + btn_bank: mute or start/stop?
- [ ] amen_cv:
- [ ] break_cv:
- [ ] sample_cv:
- [ ] clk_in:
*/

#include "clockhandling.h"
//
#include "mcp3208.h"
#include "midicallback.h"
#include "onewiremidi2.h"
#ifdef INCLUDE_MIDI
#include "midi_comm_callback.h"
#endif

typedef struct EctocoreFlash {
  uint16_t center_calibration[8];
} EctocoreFlash;

uint8_t gpio_btn_taptempo_val = 0;

// toggle the fx
void toggle_fx(uint8_t fx_num) {
  sf->fx_active[fx_num] = !sf->fx_active[fx_num];
  update_fx(fx_num);
}

const uint16_t debounce_ws2812_set_wheel_time = 10000;
uint16_t debounce_ws2812_set_wheel = 0;
uint8_t debounce_file_change = 0;

void ws2812_wheel_clear(WS2812 *ws2812) {
  debounce_ws2812_set_wheel = debounce_ws2812_set_wheel_time;
  for (uint8_t i = 0; i < 16; i++) {
    WS2812_fill(ws2812, i, 0, 0, 0);
  }
}

void ws2812_set_wheel(WS2812 *ws2812, uint16_t val, bool r, bool g, bool b) {
  debounce_ws2812_set_wheel = debounce_ws2812_set_wheel_time;
  if (val > 4079) {
    val = 4079;
  }

  int8_t filled = 0;
  while (val > 255) {
    val -= 256;
    filled++;
  }
  for (uint8_t i = 0; i < 16; i++) {
    WS2812_fill(ws2812, i, 0, 0, 0);
  }
  for (uint8_t i = 0; i < filled; i++) {
    WS2812_fill(ws2812, i, r ? 255 : 0, g ? 255 : 0, b ? 255 : 0);
  }
  if (val < 10) {
    val = 0;
  }
  WS2812_fill(ws2812, filled, r ? val : 0, g ? val : 0, b ? val : 0);
  WS2812_show(ws2812);
}

void ws2812_set_wheel_green_yellow_red(WS2812 *ws2812, uint16_t val) {
  debounce_ws2812_set_wheel = debounce_ws2812_set_wheel_time;
  if (val > 1024) {
    val = 1024;
  }

  // clear
  for (uint8_t i = 0; i < 16; i++) {
    WS2812_fill(ws2812, i, 0, 0, 0);
  }

  int8_t filled = (val * 16) / 1024;          // Scale val to range 0-8
  int16_t remainder = (val * 16) % 1024 / 4;  // Scale remainder to range 0-255
  if (remainder > 255) {
    remainder = 255;
  }
  for (uint8_t i = 0; i < filled; i++) {
    uint8_t r = 0;
    uint8_t g = 0;
    if (i < 8) {
      g = 255;
    } else if (i < 12) {
      g = 255;
      r = 255;
    } else {
      r = 255;
    }
    WS2812_fill(ws2812, i, r, g, 0);
  }

  uint8_t i = filled;
  uint8_t r = 0;
  uint8_t g = 0;
  if (i < 8) {
    g = 255;
  } else if (i < 12) {
    g = 255;
    r = 255;
  } else {
    r = 255;
  }
  r = r * remainder / 255;
  g = g * remainder / 255;
  WS2812_fill(ws2812, i, r ? remainder : 0, g ? remainder : 0, 0);
  WS2812_show(ws2812);
}

void ws2812_set_wheel_right_half(WS2812 *ws2812, uint16_t val, bool r, bool g,
                                 bool b) {
  debounce_ws2812_set_wheel = debounce_ws2812_set_wheel_time;
  if (val > 1024) {
    val = 1024;
  }

  int8_t filled = (val * 8) / 1024;          // Scale val to range 0-8
  int16_t remainder = (val * 8) % 1024 / 4;  // Scale remainder to range 0-255
  if (remainder > 255) {
    remainder = 255;
  }
  for (uint8_t i = 0; i < 8; i++) {
    WS2812_fill(ws2812, i, 0, 0, 0);
  }
  for (uint8_t i = 8; i < 8 + filled; i++) {
    WS2812_fill(ws2812, i, r ? 255 : 0, g ? 255 : 0, b ? 255 : 0);
  }

  if (remainder < 10) {
    remainder = 0;
  }

  if (filled < 8) {
    WS2812_fill(ws2812, 8 + filled, r ? remainder : 0, g ? remainder : 0,
                b ? remainder : 0);
  }

  WS2812_show(ws2812);
}

void ws2812_set_wheel_left_half(WS2812 *ws2812, uint16_t val, bool r, bool g,
                                bool b) {
  debounce_ws2812_set_wheel = debounce_ws2812_set_wheel_time;
  if (val > 1024) {
    val = 1024;
  }
  val = 1024 - val;
  int8_t filled = (val * 8) / 1024;          // Scale val to range 0-8
  int16_t remainder = (val * 8) % 1024 / 4;  // Scale remainder to range 0-255
  if (remainder > 255) {
    remainder = 255;
  }
  for (uint8_t i = 0; i < 16; i++) {
    WS2812_fill(ws2812, i, 0, 0, 0);
  }
  for (uint8_t i = 0; i < filled; i++) {
    WS2812_fill(ws2812, 7 - i, r ? 255 : 0, g ? 255 : 0, b ? 255 : 0);
  }

  if (remainder < 10) {
    remainder = 0;
  }

  if (filled < 8) {
    WS2812_fill(ws2812, 7 - filled, r ? remainder : 0, g ? remainder : 0,
                b ? remainder : 0);
  }

  WS2812_show(ws2812);
}
void break_set(int16_t val, bool ignore_taptempo_btn, bool show_wheel) {
  if (gpio_btn_taptempo_val == 0 && !ignore_taptempo_btn) {
    if (show_wheel) {
      ws2812_set_wheel_green_yellow_red(ws2812, val);
    }
    // change volume
    if (val <= 532) {
      sf->vol = val * VOLUME_STEPS / 532;
      sf->fx_active[FX_FUZZ] = 0;
      sf->fx_active[FX_SATURATE] = 0;
    } else if (val > 532 && val <= 768) {
      sf->vol = VOLUME_STEPS;
      sf->fx_active[FX_FUZZ] = 0;
      sf->fx_active[FX_SATURATE] = 1;
      sf->fx_param[FX_SATURATE][0] = 100 + (val - 532) * 128 / (768 - 532);
    } else if (val > 768) {
      sf->vol = VOLUME_STEPS;
      sf->fx_active[FX_SATURATE] = 0;
      sf->fx_active[FX_FUZZ] = 1;
      sf->fx_param[FX_FUZZ][0] = (val - 768) * 255 / (1024 - 768);
    } else {
      sf->vol = VOLUME_STEPS;
      sf->fx_active[FX_FUZZ] = 0;
      sf->fx_active[FX_SATURATE] = 0;
    }
  } else {
    if (show_wheel) {
      ws2812_set_wheel(ws2812, val * 4, false, true, true);
    }
    // // BREAK MUTE
    // if (val < 20 && !button_mute) {
    //   trigger_button_mute = true;
    //   printf("[ectocore] mute\n");
    //   WS2812_fill(ws2812, 17, 0, 0, 255);
    //   WS2812_show(ws2812);
    // } else if (val >= 20 && button_mute) {
    //   button_mute = false;
    //   WS2812_fill(ws2812, 17, 0, 255, 0);
    //   WS2812_show(ws2812);
    // }
    uint8_t u8val = val * 255 / 1024;
    // global_filter_index =
    //     ectocore_easing_filter[u8val] * (resonantfilter_fc_max) / 255;
    // printf("[ectocore] global_filter_index: %d\n",
    // global_filter_index); for (uint8_t channel = 0; channel < 2;
    // channel++) {
    //   ResonantFilter_setFilterType(resFilter[channel], 0);
    //   ResonantFilter_setFc(resFilter[channel], global_filter_index);
    // }

    if (val > 700) {
      probability_of_random_retrig = (val - 700) * (val - 700) / 500;
    } else {
      probability_of_random_retrig = 0;
    }
    if (val > 100) {
      sf->fx_param[FX_TIMESTRETCH][2] = (val - 100) * 15 / (1024 - 100);
      sf->fx_param[FX_REVERSE][2] = (val - 100) * 100 / (1024 - 100);
      sf->fx_param[FX_COMB][2] = (val - 100) * 5 / (1024 - 100);
      sf->fx_param[FX_EXPAND][2] = (val - 100) * 15 / (1024 - 100);
      sf->fx_param[FX_TAPE_STOP][2] = (val - 100) * 10 / (1024 - 100);
      // sf->fx_param[FX_BEATREPEAT][2] = (val - 100) * 10 / (1024 - 100);
      sf->fx_param[FX_BITCRUSH][2] = (val - 100) * 8 / (1024 - 100);
      sf->fx_param[FX_DELAY][2] = (val - 100) * 12 / (1024 - 100);
    } else {
      sf->fx_param[FX_REVERSE][2] = 0;
      if (sf->fx_active[FX_REVERSE]) {
        toggle_fx(FX_REVERSE);
      }
      sf->fx_param[FX_TIMESTRETCH][2] = 0;
      if (sf->fx_active[FX_TIMESTRETCH]) {
        toggle_fx(FX_TIMESTRETCH);
      }
      sf->fx_param[FX_COMB][2] = 0;
      if (sf->fx_active[FX_COMB]) {
        toggle_fx(FX_COMB);
      }
      sf->fx_param[FX_EXPAND][2] = 0;
      if (sf->fx_active[FX_EXPAND]) {
        toggle_fx(FX_EXPAND);
      }
      sf->fx_param[FX_TAPE_STOP][2] = 0;
      if (sf->fx_active[FX_TAPE_STOP]) {
        toggle_fx(FX_TAPE_STOP);
      }
      sf->fx_param[FX_BEATREPEAT][2] = 0;
      if (sf->fx_active[FX_BEATREPEAT]) {
        toggle_fx(FX_BEATREPEAT);
      }
      sf->fx_param[FX_BITCRUSH][2] = 0;
      if (sf->fx_active[FX_BITCRUSH]) {
        toggle_fx(FX_BITCRUSH);
      }
      sf->fx_param[FX_DELAY][2] = 0;
      if (sf->fx_active[FX_DELAY]) {
        toggle_fx(FX_DELAY);
      }
    }
  }
}

void dust_1() {
  // printf("[ectocore] dust_1\n");
}

void go_retrigger_2key(uint8_t key1, uint8_t key2) {
  if (retrig_vol != 1.0) {
    return;
  }
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
  sf->vol = VOLUME_STEPS - 10;
  sf->fx_active[FX_SATURATE] = true;
  sf->pitch_val_index = PITCH_VAL_MID;
  sf->stay_in_sync = true;
  uint8_t debounce_trig = 0;
  Saturation_setActive(saturation, sf->fx_active[FX_SATURATE]);

  uint8_t fx_random_on[16] = {
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  };
  uint8_t fx_random_off[16] = {
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  };
  uint8_t fx_random_max[16] = {
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  };
  uint8_t fx_random_max_off[16] = {
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  };
  uint16_t debounce_input_detection = 0;
  uint16_t debounce_mean_signal = 0;
  uint16_t mean_signal = 0;
  const uint8_t length_signal = 9;
  uint8_t magic_signal[3][10] = {
      {0, 1, 1, 0, 1, 1, 0, 1, 0, 0},
      {0, 0, 1, 0, 1, 1, 0, 0, 1, 1},
      {1, 0, 0, 1, 0, 1, 0, 1, 1, 1},
  };

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
  uint32_t gpio_btn_last_pressed[BUTTON_NUM] = {0, 0, 0, 0};
  uint8_t gpio_btn_state[BUTTON_NUM] = {0, 0, 0, 0};
  ButtonChange *button_change[BUTTON_NUM];
  for (uint8_t i = 0; i < BUTTON_NUM; i++) {
    gpio_init(gpio_btns[i]);
    gpio_set_dir(gpio_btns[i], GPIO_IN);
    gpio_pull_up(gpio_btns[i]);
    button_change[i] = ButtonChange_malloc();
  }

  gpio_init(GPIO_MODE_LEDA);
  gpio_set_dir(GPIO_MODE_LEDA, GPIO_OUT);
  gpio_put(GPIO_MODE_LEDA, 0);
  gpio_init(GPIO_MODE_LEDB);
  gpio_set_dir(GPIO_MODE_LEDB, GPIO_OUT);
  gpio_put(GPIO_MODE_LEDB, 0);

// create random dust timers
#define DUST_NUM 4
  Dust *dust[DUST_NUM];
  for (uint8_t i = 0; i < DUST_NUM; i++) {
    dust[i] = Dust_malloc();
  }
  Dust_setCallback(dust[0], dust_1);
  Dust_setDuration(dust[0], 1000 * 8);

  // create clock/midi
  Onewiremidi *onewiremidi =
      Onewiremidi_new(pio0, 3, GPIO_MIDI_IN, midi_note_on, midi_note_off,
                      midi_start, midi_continue, midi_stop, midi_timing);
  ClockInput *clockinput =
      ClockInput_create(GPIO_CLOCK_IN, clock_handling_up, clock_handling_down,
                        clock_handling_start);

  WS2812 *ws2812;
  ws2812 = WS2812_new(GPIO_WS2812, pio0, 2);
  WS2812_set_brightness(ws2812, global_brightness);

  // random colors
  for (uint8_t i = 0; i < 18; i++) {
    WS2812_fill(ws2812, i, random_integer_in_range(0, 255),
                random_integer_in_range(0, 255),
                random_integer_in_range(0, 255));
  }
  WS2812_show(ws2812);

#ifdef INCLUDE_MIDI
  tusb_init();
#endif

  Dazzle *dazzle = Dazzle_malloc();
  uint8_t led_brightness = 255;
  int8_t led_brightness_direction = 0;
  bool clock_input_absent = false;

  uint16_t debounce_startup = 10000;
  uint32_t btn_mult_on_time = 0;

  while (1) {
#ifdef INCLUDE_MIDI
    tud_task();
    midi_comm_task(midi_comm_callback_fn, NULL, NULL, NULL, NULL, NULL, NULL);
#endif
    int16_t val;
    if (debounce_startup > 0) {
      debounce_startup--;
      if (debounce_startup == 8) {
        printf("[ectocore] startup\n");
        // read flash data
        EctocoreFlash read_data;
        read_struct_from_flash(&read_data, sizeof(read_data));
        bool data_corrupted = false;
        for (uint8_t i = 0; i < 8; i++) {
          if (read_data.center_calibration[i] < 0 ||
              read_data.center_calibration[i] > 1024) {
            data_corrupted = true;
          }
        }
        if (!data_corrupted) {
          for (uint8_t i = 0; i < 8; i++) {
            sf->center_calibration[i] = read_data.center_calibration[i];
          }
        }
      } else if (debounce_startup < 8) {
        uint8_t i = debounce_startup;
        if (gpio_get(GPIO_BTN_BANK) == 0) {
          sleep_ms(1);
          sf->center_calibration[i] = MCP3208_read(mcp3208, i, false);
          if (i == 0) {
            EctocoreFlash write_data = {.center_calibration = {
                                            sf->center_calibration[0],
                                            sf->center_calibration[1],
                                            sf->center_calibration[2],
                                            sf->center_calibration[3],
                                            sf->center_calibration[4],
                                            sf->center_calibration[5],
                                            sf->center_calibration[6],
                                            sf->center_calibration[7],
                                        }};

            // make the LEDS go RED
            ws2812_wheel_clear(ws2812);
            for (uint8_t j = 0; j < 18; j++) {
              WS2812_fill(ws2812, j, 255, 0, 0);
            }
            WS2812_show(ws2812);
            sleep_ms(10);
            write_struct_to_flash(&write_data, sizeof(write_data));
            sleep_ms(1000);
            reset_usb_boot(0, 0);
          }
        }
        printf("[ectocore] calibrate %d=%d,", i, sf->center_calibration[i]);
      }
    }

    ClockInput_update(clockinput);
    if (clock_in_do) {
      if (ClockInput_timeSinceLast(clockinput) > 1000000) {
        clock_input_absent = true;
      }
    }
    Onewiremidi_receive(onewiremidi);

    if (random_integer_in_range(1, 2000000) < probability_of_random_retrig) {
      printf("[ecotocre] random retrigger\n");
      sf->do_retrig_pitch_changes = (random_integer_in_range(1, 10) < 5);
      go_retrigger_2key(random_integer_in_range(0, 15),
                        random_integer_in_range(0, 15));
    }

    // update dusts
    for (uint8_t i = 0; i < DUST_NUM; i++) {
      Dust_update(dust[i]);
    }

    if (debounce_mean_signal > 0 && mean_signal > 0) {
      debounce_mean_signal--;
    } else {
      // calculate mean signal
      int16_t total_mean_signal = 0;
      uint8_t total_signals_sent = 0;
      for (uint8_t j = 0; j < 3; j++) {
        if (!cv_plugged[j]) {
          total_signals_sent++;
          for (uint8_t i = 0; i < length_signal; i++) {
            gpio_put(GPIO_INPUTDETECT, magic_signal[j][i]);
            sleep_us(6);
            total_mean_signal += MCP3208_read(mcp3208, cv_signals[j], false);
          }
        }
      }
      if (total_signals_sent > 0) {
        mean_signal = total_mean_signal / (total_signals_sent * length_signal);
        // printf("[ectocore] mean_signal: %d\n", mean_signal);
      }
      debounce_mean_signal = 10000;
    }

    if (debounce_input_detection > 0) {
      debounce_input_detection--;
    } else if (mean_signal > 0) {
      // input detection
      bool found_change = false;
      int16_t val_input;
      uint8_t response_signal[3][10] = {
          {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
          {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
          {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      };

      for (uint8_t j = 0; j < 3; j++) {
        for (uint8_t i = 0; i < length_signal; i++) {
          gpio_put(GPIO_INPUTDETECT, magic_signal[j][i]);
          sleep_us(6);
          val_input = MCP3208_read(mcp3208, cv_signals[j], false);
          if (val_input > mean_signal) {
            response_signal[j][i] = 1;
          }
          // if (j == 0) {
          //   printf("%d ", val_input);
          // }
        }
        // if (j == 0) {
        //   printf("\n");
        // }
      }
      bool is_signal[3] = {true, true, true};
      for (uint8_t j = 0; j < 3; j++) {
        for (uint8_t i = 0; i < length_signal; i++) {
          if (response_signal[j][i] != magic_signal[j][i]) {
            is_signal[j] = false;
            break;
          }
        }
      }
      for (uint8_t j = 0; j < 3; j++) {
        if (!is_signal[j] && !cv_plugged[j]) {
          printf("[ectocore] cv_%d plugged\n", j);
          debounce_mean_signal = 10;
        } else if (is_signal[j] && cv_plugged[j]) {
          printf("[ectocore] cv_%d unplugged\n", j);
          debounce_mean_signal = 10;
        }
        cv_plugged[j] = !is_signal[j];
      }
      debounce_input_detection = 100;
    }

    // update the cv for each channel
    for (uint8_t i = 0; i < 3; i++) {
      if (cv_plugged[i]) {
        // firist figure out CV values
        val = MCP3208_read(mcp3208, cv_signals[i], false) - 512;
        if (i < 2) {
          // read in the attenuator
          int16_t val_attenuate = MCP3208_read(mcp3208, cv_attenuate[i], false);
          if (val_attenuate > 520) {
            // linear interpolation
            val = val * (val_attenuate - 520) / (1024 - 520);
            cv_values[i] = val;
          } else if (val_attenuate < 500) {
            // TODO: add random noise
            cv_values[i] = val;
          }
        } else {
          cv_values[i] = val;
        }
        // then do something based on the CV value
        if (i == CV_AMEN) {
          // change the position base on the CV value
          cv_beat_current_override = linlin(cv_values[i], -512, 512, 0,
                                            banks[sel_bank_cur]
                                                ->sample[sel_sample_cur]
                                                .snd[FILEZERO]
                                                ->slice_num);
        } else if (i == CV_BREAK) {
          // update the break stuff
          break_set(linlin(cv_values[i], -512, 512, 0, 1024), true, false);
        } else if (i == CV_SAMPLE) {
          // change the sample based on the cv value
          if (fil_current_change != true && debounce_file_change == 0) {
            sel_sample_next = linlin(cv_values[i], 0, 1024, 0,
                                     banks[sel_bank_cur]->num_samples);
            if (sel_sample_next != sel_sample_cur) {
              printf("[ectocore] switch sample %d\n", sel_sample_next);
              fil_current_change = true;
              debounce_file_change = 100;
            }
          }
        }
      }
    }
    if (debounce_file_change > 0) {
      debounce_file_change--;
    }

    if (clock_out_ready) {
      clock_out_ready = false;
      uint16_t j = beat_current;
      while (j > banks[sel_bank_cur]
                     ->sample[sel_sample_cur]
                     .snd[FILEZERO]
                     ->slice_num) {
        j -= banks[sel_bank_cur]
                 ->sample[sel_sample_cur]
                 .snd[FILEZERO]
                 ->slice_num;
      }
      if (ectocore_trigger_mode == TRIGGER_MODE_KICK) {
        if (banks[sel_bank_cur]
                    ->sample[sel_sample_cur]
                    .snd[FILEZERO]
                    ->slice_type[j] == 1 ||
            banks[sel_bank_cur]
                    ->sample[sel_sample_cur]
                    .snd[FILEZERO]
                    ->slice_type[j] == 3) {
          gpio_put(GPIO_TRIG_OUT, 1);
          debounce_trig = 100;
        }
      } else if (ectocore_trigger_mode == TRIGGER_MODE_SNARE) {
        if (banks[sel_bank_cur]
                    ->sample[sel_sample_cur]
                    .snd[FILEZERO]
                    ->slice_type[j] == 2 ||
            banks[sel_bank_cur]
                    ->sample[sel_sample_cur]
                    .snd[FILEZERO]
                    ->slice_type[j] == 3) {
          gpio_put(GPIO_TRIG_OUT, 1);
          debounce_trig = 100;
        }
      } else if (ectocore_trigger_mode == TRIGGER_MODE_HH) {
      } else if (ectocore_trigger_mode == TRIGGER_MODE_RANDOM) {
        if (random_integer_in_range(1, 100) < 20) {
          gpio_put(GPIO_TRIG_OUT, 1);
          debounce_trig = 100;
        }
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
        printf("version=v2.7.7\n");
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
#else
#ifdef PRINT_AUDIOBLOCKDROPS
    // random stuff
    if (random_integer_in_range(1, 10000) < 80) {
      // printf("random retrig\n");
      key_do_jump(random_integer_in_range(0, 15));
    } else if (random_integer_in_range(1, 10000) < 5) {
      // printf("random retrigger\n");
      go_retrigger_2key(random_integer_in_range(0, 15),
                        random_integer_in_range(0, 15));
    }
#endif
#endif

    gpio_btn_taptempo_val = gpio_get(GPIO_BTN_TAPTEMPO);

    if (gpio_btn_taptempo_val == 0 && !btn_taptempo_on) {
      if (clock_input_absent && clock_in_activator >= 3) {
        printf("reseting clock\n");
        clock_in_ready = false;
        clock_in_activator = 0;
        clock_in_do = false;
      }
      btn_taptempo_on = true;
      gpio_put(GPIO_LED_TAPTEMPO, 0);
      val = TapTempo_tap(taptempo);
      if (val > 0) {
        printf("[ectocore] tap bpm -> %d\n", val);
        sf->bpm_tempo = val;
      }
    } else if (gpio_btn_taptempo_val == 1 && btn_taptempo_on) {
      btn_taptempo_on = false;
      gpio_put(GPIO_LED_TAPTEMPO, 1);
    }

    for (uint8_t i = 0; i < KNOB_NUM; i++) {
      val = KnobChange_update(knob_change[i],
                              MCP3208_read(mcp3208, knob_gpio[i], false));
      if (val < 0) {
        continue;
      }
      // normalize val based on center calibration
      if (val < sf->center_calibration[knob_gpio[i]]) {
        val = linlin(val, 0, sf->center_calibration[knob_gpio[i]], 0, 512);
      } else {
        val = linlin(val - sf->center_calibration[knob_gpio[i]], 0,
                     1024 - sf->center_calibration[knob_gpio[i]], 0, 512) +
              512;
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
                if (sel_bank_next != sel_bank_cur) {
                  sel_sample_next =
                      sel_sample_cur % banks[sel_bank_next]->num_samples;
                  fil_current_change = true;
                  printf("[ectocore] switch bank %d\n", val);
                  ws2812_wheel_clear(ws2812);
                  WS2812_fill(ws2812, val, 255, 0, 0);
                  WS2812_show(ws2812);
                }
                break;
              }
              bank_num++;
            }
          }
        } else {
          // sample selection
          printf("[ectocore] switch sample %d\n", val);
          val = (val * banks[sel_bank_next]->num_samples) / 1024;
          ws2812_wheel_clear(ws2812);
          WS2812_fill(ws2812, val, 0, 255, 255);
          WS2812_show(ws2812);
          if (val != sel_sample_cur) {
            sel_sample_next = val;
            fil_current_change = true;
            printf("[ectocore] switchsample val=%d\n", val);
          }
        }
      } else if (knob_gpio[i] == MCP_KNOB_BREAK) {
        printf("[ectocore] knob_break %d\n", val);
        break_set(val, false, true);
      } else if (knob_gpio[i] == MCP_KNOB_AMEN) {
        printf("[ectocore] knob_amen %d\n", val);
        if (gpio_btn_taptempo_val == 0) {
          // TODO: change the filter cutoff!
          global_filter_index = val * (resonantfilter_fc_max) / 1024;
          printf("[ectocore] global_filter_index: %d\n", global_filter_index);
          if (val > 960) {
            global_filter_index = resonantfilter_fc_max;
          }
          for (uint8_t channel = 0; channel < 2; channel++) {
            ResonantFilter_setFilterType(resFilter[channel], 0);
            ResonantFilter_setFc(resFilter[channel], global_filter_index);
          }
        } else {
          if (!cv_plugged[CV_AMEN]) {
            if (val < 57) {
              ws2812_set_wheel(ws2812, val * 4, 12, 123, 32);
              WS2812_show(ws2812);
              // disable random sequence mode
              random_sequence_length = 0;
              do_retrig_at_end_of_phrase = false;
            } else if (val < 966) {
              ws2812_set_wheel(ws2812, val * 4, 60, 63, 32);
              WS2812_show(ws2812);
              uint8_t sequence_lengths[11] = {
                  1, 2, 4, 6, 8, 12, 16, 24, 32, 48, 64,
              };
              random_sequence_length =
                  sequence_lengths[((int16_t)(val - 57) * 11 / (966 - 57)) %
                                   11];
              printf("[ectocore] random_sequence_length %d\n",
                     random_sequence_length);
              do_retrig_at_end_of_phrase = false;
            } else {
              ws2812_set_wheel(ws2812, val * 4, 120, 3, 32);
              WS2812_show(ws2812);
              printf("[ectocore] regen sequence\n");
              // generative mode + generate new sequence
              for (uint8_t i = 0; i < 64; i++) {
                random_sequence_arr[i] = random_integer_in_range(0, 64);
              }
              for (uint8_t i = 0; i < 16; i++) {
                ws2812_set_wheel(ws2812, i, 0, 255, 0);
              }
              WS2812_show(ws2812);
              sleep_ms(200);
              random_sequence_length = 8;
              do_retrig_at_end_of_phrase = true;
            }
          } else {
            random_sequence_length = 0;
            do_retrig_at_end_of_phrase = false;
          }
        }
      } else if (knob_gpio[i] == MCP_ATTEN_BREAK) {
        printf("[ectocore] knob_break_atten %d\n", val);
      } else if (knob_gpio[i] == MCP_ATTEN_AMEN) {
        printf("[ectocore] knob_amen_atten %d\n", val);
        if (val < 512) {
          sf->stay_in_sync = false;
          probability_of_random_jump = (512 - val) * 100 / 512;
          ws2812_set_wheel_left_half(ws2812, 2 * val, true, false, true);
        } else {
          sf->stay_in_sync = true;
          probability_of_random_jump = (val - 512) * 100 / 512;
          ws2812_set_wheel_right_half(ws2812, 2 * (val - 512), true, false,
                                      true);
        }
        // if (val < 10 && !playback_stopped) {
        //   // if (!button_mute) trigger_button_mute = true;
        //   // do_stop_playback = true;
        //   // WS2812_fill(ws2812, 17, 255, 0, 0);
        //   // WS2812_show(ws2812);
        // } else if (val > 10 && playback_stopped) {
        //   // do_restart_playback = true;
        //   // button_mute = false;
        //   // WS2812_fill(ws2812, 17, 0, 255, 0);
        //   // WS2812_show(ws2812);
        // } else {
        // }
      }
    }

    // button selection
    for (uint8_t i = 0; i < BUTTON_NUM; i++) {
      val = ButtonChange_update(button_change[i], gpio_get(gpio_btns[i]));
      if (val < 0) {
        continue;
      }
      val = 1 - val;
      gpio_btn_state[i] = val;
      if (val) {
        gpio_btn_last_pressed[i] = to_ms_since_boot(get_absolute_time());
      }
      if (gpio_btns[i] == GPIO_BTN_MODE) {
        printf("[ectocore] btn_mode %d\n", val);

        if (val == 1) {
          if (ectocore_trigger_mode < 4 - 1) {
            ectocore_trigger_mode++;
          } else {
            ectocore_trigger_mode = 0;
          }
          switch (ectocore_trigger_mode) {
            case TRIGGER_MODE_KICK:
              printf("[ectocore] trigger mode: kick\n");
              gpio_put(GPIO_MODE_LEDA, 0);
              gpio_put(GPIO_MODE_LEDB, 0);
              break;
            case TRIGGER_MODE_SNARE:
              printf("[ectocore] trigger mode: snare\n");
              gpio_put(GPIO_MODE_LEDA, 1);
              gpio_put(GPIO_MODE_LEDB, 0);
              break;
            case TRIGGER_MODE_HH:
              printf("[ectocore] trigger mode: hh\n");
              gpio_put(GPIO_MODE_LEDA, 0);
              gpio_put(GPIO_MODE_LEDB, 1);
              break;
            case TRIGGER_MODE_RANDOM:
              printf("[ectocore] trigger mode: random\n");
              gpio_put(GPIO_MODE_LEDA, 1);
              gpio_put(GPIO_MODE_LEDB, 1);
              break;
          }
        }
      } else if (gpio_btns[i] == GPIO_BTN_BANK) {
        printf("[ectocore] btn_bank %d\n", val);
        if (val == 0) {
          if (to_ms_since_boot(get_absolute_time()) - gpio_btn_last_pressed[i] <
              200) {
            // "tap"
            // switch the bank by one
            if (banks_with_samples_num > 1) {
              for (uint8_t j = 1; j < banks_with_samples_num; j++) {
                uint8_t bank_num = (sel_bank_cur + j) % banks_with_samples_num;
                if (banks[bank_num]->num_samples > 0) {
                  sel_bank_next = bank_num;
                  if (sel_bank_next != sel_bank_cur) {
                    sel_sample_next =
                        sel_sample_cur % banks[sel_bank_next]->num_samples;
                    fil_current_change = true;
                    printf("[ectocore] switch bank %d\n", sel_bank_next);
                    ws2812_wheel_clear(ws2812);
                    WS2812_fill(ws2812, sel_bank_next, 255, 0, 0);
                    WS2812_show(ws2812);
                  }
                  break;
                }
              }
            }
          }
        }
      } else if (gpio_btns[i] == GPIO_BTN_MULT) {
        if (val) {
          printf("[ectocore] btn_mult %d\n", val);
          btn_mult_on_time = to_ms_since_boot(get_absolute_time());
        } else {
          if (to_ms_since_boot(get_absolute_time()) - btn_mult_on_time < 200) {
            // tap
            printf("[ectocore] btn_mult tap\n");
            if (ectocore_clock_selected_division > 0)
              ectocore_clock_selected_division--;
          } else {
            // hold
            printf("[ectocore] btn_mult hold\n");
            if (ectocore_clock_selected_division <
                ECTOCORE_CLOCK_NUM_DIVISIONS - 1)
              ectocore_clock_selected_division++;
          }
        }
      }
      // check for reset
      if (gpio_btn_state[GPIO_BTN_BANK] > 0 &&
          gpio_btn_state[GPIO_BTN_MODE] > 0 &&
          gpio_btn_state[GPIO_BTN_MULT] > 0) {
        reset_usb_boot(0, 0);
      }
    }

    // updating the random fx
    if (clock_did_activate) {
      clock_did_activate = false;
      for (uint8_t i = 0; i < 16; i++) {
        if (sf->fx_param[i][2] > 0) {
          if (sf->fx_active[i]) {
            fx_random_on[i]++;
            fx_random_off[i] = 0;
            if (fx_random_on[i] >= fx_random_max[i]) {
              toggle_fx(i);
              // printf("[zeptocore] random fx: %d %d\n", i, sf->fx_active[i]);
              fx_random_max_off[i] = random_integer_in_range(2, 6);
              if (i == FX_TIMESTRETCH) {
                fx_random_max_off[i] = random_integer_in_range(16, 32);
              } else if (i == FX_REVERSE) {
                fx_random_max_off[i] = random_integer_in_range(1, 4);
              } else if (i == FX_COMB) {
                fx_random_max_off[i] = random_integer_in_range(1, 4);
              } else if (i == FX_EXPAND) {
                fx_random_max_off[i] = random_integer_in_range(1, 4);
              } else if (i == FX_TAPE_STOP) {
                fx_random_max_off[i] = random_integer_in_range(8, 16);
              }
            }
          } else {
            fx_random_off[i]++;
            fx_random_on[i] = 0;
            if (fx_random_off[i] >= fx_random_max_off[i]) {
              if (random_integer_in_range(0, 100) < sf->fx_param[i][2] / 4) {
                fx_random_max[i] = random_integer_in_range(4, 16);
                if (i == FX_TIMESTRETCH) {
                  fx_random_max[i] = random_integer_in_range(16, 32);
                } else if (i == FX_REVERSE) {
                  fx_random_max[i] = random_integer_in_range(1, 4);
                } else if (i == FX_COMB) {
                  fx_random_max[i] = random_integer_in_range(1, 4);
                } else if (i == FX_EXPAND) {
                  fx_random_max[i] = random_integer_in_range(2, 4);
                } else if (i == FX_TAPE_STOP) {
                  fx_random_max[i] = random_integer_in_range(4, 12);
                } else if (i == FX_BEATREPEAT) {
                  fx_random_max[i] = random_integer_in_range(1, 3);
                }
                sf->fx_param[i][1] = random_integer_in_range(0, 200);
                sf->fx_param[i][2] = random_integer_in_range(0, 200);
                toggle_fx(i);
                Dazzle_start(dazzle, i % 3);
                // TODO: also randomize the parameters?
                // printf("[zeptocore] random fx: %d %d\n", i,
                // sf->fx_active[i]);
              }
            }
          }
        }
      }
    }

    // load the new sample if variation changed
    if (sel_variation_next != sel_variation) {
      if (!audio_callback_in_mute) {
        while (!sync_using_sdcard) {
          sleep_us(250);
        }
        while (sync_using_sdcard) {
          sleep_us(250);
        }
      }
      sync_using_sdcard = true;
      // measure the time it takes
      uint32_t time_start = time_us_32();
      FRESULT fr = f_close(&fil_current);
      if (fr != FR_OK) {
        printf("[zeptocore] f_close error: %s\n", FRESULT_str(fr));
      }
      sprintf(fil_current_name, "bank%d/%d.%d.wav", sel_bank_cur,
              sel_sample_cur, sel_variation_next);
      fr = f_open(&fil_current, fil_current_name, FA_READ);
      if (fr != FR_OK) {
        printf("[zeptocore] f_open error: %s\n", FRESULT_str(fr));
      }

      // TODO: fix this
      // if sel_variation_next == 0
      phases[0] = round(
          ((float)phases[0] * (float)sel_variation_scale[sel_variation_next]) /
          (float)sel_variation_scale[sel_variation]);

      sel_variation = sel_variation_next;
      sync_using_sdcard = false;
      printf("[zeptocore] loading new sample variation took %d us\n",
             time_us_32() - time_start);
    }

    // updating the leds
    if (debounce_ws2812_set_wheel > 0) {
      debounce_ws2812_set_wheel--;
    } else {
      if (Dazzle_update(dazzle, ws2812)) {
        // dazzling
      } else {
        // highlight the current sample in the leds
        for (uint8_t i = 0; i < 16; i++) {
          WS2812_fill(ws2812, i, 0, 0, 0);
        }
        if (retrig_beat_num > 0 && retrig_beat_num % 2 == 0) {
          for (uint8_t i = 0; i < 16; i++) {
            uint8_t r, g, b;
            hue_to_rgb((float)255 / (retrig_beat_num + 1), &r, &g, &b);
            WS2812_fill(ws2812, i, r * led_brightness / 255,
                        g * led_brightness / 255, b * led_brightness / 255);
          }
        }
        WS2812_fill(ws2812,
                    banks[sel_bank_cur]
                            ->sample[sel_sample_cur]
                            .snd[FILEZERO]
                            ->slice_current %
                        16,
                    50 * led_brightness / 255, 190 * led_brightness / 255,
                    255 * led_brightness / 255);
        if (sf->fx_active[FX_COMB]) {
          for (uint8_t i = 2; i < 14; i += 2) {
            WS2812_fill(ws2812,
                        (banks[sel_bank_cur]
                             ->sample[sel_sample_cur]
                             .snd[FILEZERO]
                             ->slice_current +
                         i) %
                            16,
                        50 / 8 * led_brightness / 255,
                        190 / 8 * led_brightness / 255,
                        255 / 8 * led_brightness / 255);
          }
        }
        // add flourishes if effects are on
        if (sf->fx_active[FX_REVERSE]) {
          WS2812_fill(ws2812,
                      (banks[sel_bank_cur]
                           ->sample[sel_sample_cur]
                           .snd[FILEZERO]
                           ->slice_current +
                       1) %
                          16,
                      50 / 2 * led_brightness / 255,
                      190 / 2 * led_brightness / 255,
                      255 / 2 * led_brightness / 255);
          WS2812_fill(ws2812,
                      (banks[sel_bank_cur]
                           ->sample[sel_sample_cur]
                           .snd[FILEZERO]
                           ->slice_current +
                       2) %
                          16,
                      50 / 4 * led_brightness / 255,
                      190 / 4 * led_brightness / 255,
                      255 / 4 * led_brightness / 255);
          WS2812_fill(ws2812,
                      (banks[sel_bank_cur]
                           ->sample[sel_sample_cur]
                           .snd[FILEZERO]
                           ->slice_current +
                       3) %
                          16,
                      50 / 8 * led_brightness / 255,
                      190 / 8 * led_brightness / 255,
                      255 / 8 * led_brightness / 255);
        }
        if (sf->fx_active[FX_DELAY]) {
          WS2812_fill(ws2812,
                      (banks[sel_bank_cur]
                           ->sample[sel_sample_cur]
                           .snd[FILEZERO]
                           ->slice_current +
                       8) %
                          16,
                      50 / 2 * led_brightness / 255,
                      190 / 2 * led_brightness / 255,
                      255 / 2 * led_brightness / 255);
        }
        if (sf->fx_active[FX_TIMESTRETCH]) {
          led_brightness_direction = -1;
        } else {
          led_brightness_direction = 0;
          led_brightness = 255;
        }

        if (led_brightness + led_brightness_direction > 255 ||
            led_brightness - led_brightness_direction < 0) {
          led_brightness_direction = -led_brightness_direction;
        }
        led_brightness += led_brightness_direction;

        WS2812_show(ws2812);
      }
      sleep_ms(1);
    }
  }
}