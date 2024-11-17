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
#include "break_knob.h"

#define KNOB_ATTEN_ZERO_WIDTH 80
#define DEBOUNCE_FILE_SWITCH 500

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

void ws2812_wheel_clear(WS2812 *ws2812) {
  debounce_ws2812_set_wheel = debounce_ws2812_set_wheel_time;
  for (uint8_t i = 0; i < 16; i++) {
    WS2812_fill(ws2812, i, 0, 0, 0);
  }
}

void ws2812_set_wheel_section(WS2812 *ws2812, uint8_t val, uint8_t max,
                              uint8_t r, uint8_t g, uint8_t b) {
  debounce_ws2812_set_wheel = debounce_ws2812_set_wheel_time;
  ws2812_wheel_clear(ws2812);

  static uint8_t n = 16;
  bool rhythm[n];
  uint8_t arr[n];

  generate_euclidean_rhythm(n, max, 0, rhythm);

  uint8_t total = 0;
  for (int i = 0; i < n; i++) {
    arr[i] = rhythm[i];
    total += arr[i];
  }
  if (total == 0) {
    return;
  }

  // rotate until i=0 has a 1 in it
  while (arr[0] == 0) {
    bool tmp = arr[n - 1];
    for (int j = n - 1; j > 0; j--) {
      arr[j] = arr[j - 1];
    }
    arr[0] = tmp;
  }

  // cumulative sum of arr
  arr[0] = 0;
  for (int i = 1; i < n; i++) {
    arr[i] += arr[i - 1];
  }

  for (uint8_t i = 0; i < 16; i++) {
    if (arr[i] == val) {
      WS2812_fill(ws2812, i, r, g, b);
    }
  }
}

void ws2812_set_wheel_euclidean(WS2812 *ws2812, uint8_t val, uint8_t r,
                                uint8_t g, uint8_t b) {
  debounce_ws2812_set_wheel = debounce_ws2812_set_wheel_time;
  ws2812_wheel_clear(ws2812);
  bool rhythm[16];
  while (val > 16) {
    val = val - 16;
    // swap r, g, b
    uint8_t temp = r;
    r = g;
    g = b;
    b = temp;
  }
  uint8_t k = val;
  generate_euclidean_rhythm(16, k, (16 / k) / 2 + 1, rhythm);
  for (uint8_t i = 0; i < 16; i++) {
    if (rhythm[i]) {
      WS2812_fill(ws2812, i, r, g, b);
    }
  }

  WS2812_show(ws2812);
}

void ws2812_set_wheel_start_stop(WS2812 *ws2812, uint8_t start, uint8_t stop,
                                 uint8_t r, uint8_t g, uint8_t b) {
  debounce_ws2812_set_wheel = debounce_ws2812_set_wheel_time;
  // start is 0-16, stop is 0-16
  ws2812_wheel_clear(ws2812);
  for (uint8_t i = start; i <= stop; i++) {
    WS2812_fill(ws2812, i, r, g, b);
  }
  WS2812_show(ws2812);
}

// cv_start and cv_stop are between 0 and 1000
int16_t cv_start = -1;
int16_t cv_stop = -1;
void set_cv_start_stop(int16_t knob1, int16_t knob2) {
  int16_t cv_start_old = cv_start;
  int16_t cv_stop_old = cv_stop;
  // start and stop are knob values between 0-1024
  cv_start = knob1 * 1000 / 1024;
  if (knob2 < 512) {
    cv_stop = cv_start + ((512 - knob2) * 1000 / 512);
  } else {
    cv_stop = cv_start + (knob2 - 512) * 1000 / 512;
  }
  if (cv_stop > 1000) {
    cv_stop = 1000;
  }
  if (cv_start != cv_start_old || cv_stop != cv_stop_old) {
    printf("[ectocore] cv_start=%d, cv_stop=%d\n", cv_start, cv_stop);
    if (knob2 < 512) {
      ws2812_set_wheel_start_stop(ws2812, cv_start * 16 / 1000,
                                  cv_stop * 16 / 1000, 100, 200, 255);
    } else {
      ws2812_set_wheel_start_stop(ws2812, cv_start * 16 / 1000,
                                  cv_stop * 16 / 1000, 200, 100, 255);
    }
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

void ws2812_set_wheel2(WS2812 *ws2812, uint16_t val, uint8_t r, uint8_t g,
                       uint8_t b) {
  debounce_ws2812_set_wheel = debounce_ws2812_set_wheel_time;
  if (val > 1024) {
    val = 1024;
  }

  float amt = ((float)val) * 16.0 / 1024.0;

  uint8_t filled = floor(amt);
  uint8_t leftover = (amt - filled) * 255;

  for (uint8_t i = 0; i < 16; i++) {
    if (i < filled) {
      WS2812_fill(ws2812, i, r, g, b);
    } else if (i == filled) {
      WS2812_fill(ws2812, i, r * leftover / 255, g * leftover / 255,
                  b * leftover / 255);
    } else {
      WS2812_fill(ws2812, i, 0, 0, 0);
    }
  }
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

bool break_set(int16_t val, bool ignore_taptempo_btn, bool show_wheel) {
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
      sf->fx_param[FX_SATURATE][0] = 25 + (val - 532) * 128 / (768 - 532);
    } else if (val > 768) {
      sf->vol = VOLUME_STEPS;
      sf->fx_active[FX_SATURATE] = 0;
      sf->fx_active[FX_FUZZ] = 1;
      sf->fx_param[FX_FUZZ][1] = 125;
      sf->fx_param[FX_FUZZ][0] = (val - 768) * 255 / (1024 - 768);
    } else {
      sf->vol = VOLUME_STEPS;
      sf->fx_active[FX_FUZZ] = 0;
      sf->fx_active[FX_SATURATE] = 0;
    }
    return true;
  }
  break_knob_set_point = val;
  if (show_wheel) {
    uint8_t r, g, b;
    int16_t val2 = val / 4;
    hue_to_rgb2(val2, &r, &g, &b);
    ws2812_set_wheel2(ws2812, val, r, g, b);
  }
}

void dust_1() {
  // printf("[ectocore] dust_1\n");
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

  uint16_t debounce_input_detection = 0;
  uint16_t debounce_mean_signal = 0;
  uint16_t mean_signal = 0;
  const uint8_t length_signal = 9;
  uint8_t magic_signal[3][10] = {
      {0, 1, 1, 0, 1, 1, 0, 1, 0, 0},
      {0, 0, 1, 0, 1, 1, 0, 0, 1, 1},
      {1, 0, 0, 1, 0, 1, 0, 1, 1, 1},
  };
  bool cv_was_unplugged[3] = {false, false, false};

  // update the knobs
#define KNOB_NUM 5
#define KNOB_BREAK 0
#define KNOB_BREAK_ATTEN 1
#define KNOB_AMEN 2
#define KNOB_AMEN_ATTEN 3
#define KNOB_SAMPLE 4
  uint8_t knob_gpio[KNOB_NUM] = {
      MCP_KNOB_BREAK, MCP_ATTEN_BREAK, MCP_KNOB_AMEN,
      MCP_ATTEN_AMEN, MCP_KNOB_SAMPLE,
  };
  int16_t knob_val[KNOB_NUM] = {0, 0, 0, 0, 0};
  KnobChange *knob_change[KNOB_NUM];
  for (uint8_t i = 0; i < KNOB_NUM; i++) {
    knob_change[i] = KnobChange_malloc(6);
  }

#define BUTTON_NUM 4
#define BTN_MODE 0
#define BTN_MULT 1
#define BTN_BANK 2
#define BTN_TAPTEMPO 3
  const uint8_t gpio_btns[BUTTON_NUM] = {
      GPIO_BTN_MODE,
      GPIO_BTN_MULT,
      GPIO_BTN_BANK,
      GPIO_BTN_TAPTEMPO,
  };
  uint32_t gpio_btn_last_pressed[BUTTON_NUM] = {0, 0, 0, 0};
  uint32_t gpio_btn_held_time[BUTTON_NUM] = {0, 0, 0, 0};
  uint8_t gpio_btn_state[BUTTON_NUM] = {0, 0, 0, 0};
  ButtonChange *button_change[BUTTON_NUM];
  for (uint8_t i = 0; i < BUTTON_NUM; i++) {
    gpio_init(gpio_btns[i]);
    gpio_set_dir(gpio_btns[i], GPIO_IN);
    gpio_pull_up(gpio_btns[i]);
    button_change[i] = ButtonChange_malloc();
  }

#ifdef ECTOCORE_VERSION_3
  gpio_init(GPIO_MODE_LEDA);
  gpio_set_dir(GPIO_MODE_LEDA, GPIO_OUT);
  gpio_put(GPIO_MODE_LEDA, 0);
  gpio_init(GPIO_MODE_LEDB);
  gpio_set_dir(GPIO_MODE_LEDB, GPIO_OUT);
  gpio_put(GPIO_MODE_LEDB, 0);
#endif
#ifdef ECTOCORE_VERSION_4
  const uint8_t gpio_mode_leds[4] = {
      GPIO_MODE_1,
      GPIO_MODE_2,
      GPIO_MODE_3,
      GPIO_MODE_4,
  };
  for (uint8_t i = 0; i < 4; i++) {
    gpio_init(gpio_mode_leds[i]);
    gpio_set_dir(gpio_mode_leds[i], GPIO_OUT);
    gpio_put(gpio_mode_leds[i], 1);
  }
  gpio_put(gpio_mode_leds[0], 0);
#endif

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
  bool clock_input_absent = true;

  uint16_t debounce_startup = 400;
  if (do_calibration_mode) {
    debounce_startup = 110;
  }
  uint32_t btn_mult_on_time = 0;
  uint32_t btn_mult_hold_time = 0;
  uint32_t debounce_file_switching = 0;
  uint8_t sel_bank_next_new = 0;
  uint8_t sel_sample_next_new = 0;

  for (uint8_t i = 0; i < 64; i++) {
    random_sequence_arr[i] = random_integer_in_range(0, 64);
  }

#ifdef INCLUDE_CUEDSOUNDS
  mute_soft = true;
#endif

  int cv_amen_last_value = 0;

  while (1) {
#ifdef INCLUDE_MIDI
    tud_task();
    midi_comm_task(midi_comm_callback_fn, NULL, NULL, NULL, NULL, NULL, NULL);
#endif
    int16_t val;
    if (debounce_startup > 0) {
      debounce_startup--;
      if (debounce_startup == 10) {
        printf("clock_start_stop_sync: %d\n", clock_start_stop_sync);
      } else if (debounce_startup == 9) {
        printf("global_brightness: %d\n", global_brightness);
      } else if (debounce_startup == 7) {
#ifdef INCLUDE_CUEDSOUNDS
        cuedsounds_do_play = random_integer_in_range(0, CUEDSOUNDS_FILES - 1);
#endif
      } else if (debounce_startup == 108) {
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
      } else if (debounce_startup >= 100 && debounce_startup < 108) {
        uint8_t i = debounce_startup - 100;
        if (do_calibration_mode) {
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

            printf("[ectocore] write calibration\n");
            uint16_t flash_time = 250;
            for (uint8_t ii = 0; ii < 20; ii++) {
              // make the LEDS go RED
              for (uint8_t j = 0; j < 20; j++) {
                WS2812_fill(ws2812, j, 0, 255, 0);
              }
              WS2812_show(ws2812);
              sleep_ms(flash_time);
              for (uint8_t j = 0; j < 20; j++) {
                WS2812_fill(ws2812, j, 0, 0, 0);
              }
              WS2812_show(ws2812);
              sleep_ms(flash_time);

              flash_time = flash_time * 90 / 100;
              if (flash_time < 10) {
                break;
              }
            }
            watchdog_reboot(0, SRAM_END, 1900);
            sleep_ms(10);
            write_struct_to_flash(&write_data, sizeof(write_data));
            sleep_ms(3000);
            for (;;) {
              __wfi();
            }
          }
        }
        printf("[ectocore] calibrate %d=%d,", i, sf->center_calibration[i]);
      }
    }

    ClockInput_update(clockinput);
    if (clock_in_do || !clock_input_absent) {
      bool clock_input_absent_new =
          ClockInput_timeSinceLast(clockinput) > 1000000;
      if (clock_input_absent_new != clock_input_absent) {
        clock_input_absent = clock_input_absent_new;
        if (clock_input_absent) {
          printf("[ectocore] clock input absent\n");
        } else {
          printf("[ectocore] clock input present\n");
        }
      }
    }
    Onewiremidi_receive(onewiremidi);

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
          cv_was_unplugged[j] = true;
        }
        cv_plugged[j] = !is_signal[j];
      }
      debounce_input_detection = 100;
    }

    // update the cv for each channel
    for (uint8_t i = 0; i < 3; i++) {
      if (cv_plugged[i]) {
        // collect out CV values
        val = MCP3208_read(mcp3208, cv_signals[i], false) - 512;
        // then do something based on the CV value
        if (i == CV_AMEN) {
          if (cv_start == -1 || cv_stop == -1) {
            set_cv_start_stop(knob_val[KNOB_AMEN], knob_val[KNOB_AMEN_ATTEN]);
          }
          // possibly add random value
          if (knob_val[KNOB_AMEN_ATTEN] < 512) {
            int16_t range = (512 - knob_val[KNOB_AMEN_ATTEN]) * 256;
            val = val + random_integer_in_range(-1 * range, range);
            while (val < -512) {
              val += 512;
            }
            while (val > 512) {
              val -= 512;
            }
          }
          bool has_changed =
              (cv_amen_last_value - val > 3 || val - cv_amen_last_value > 3);
          cv_amen_last_value = val;
          if ((global_amen_cv_behavior == AMEN_CV_BEHAVIOR_JUMP &&
               has_changed) ||
              global_amen_cv_behavior == AMEN_CV_BEHAVIOR_REPEAT) {
            int16_t start_cv = 0;
            if (global_amen_cv_bipolar) {
              start_cv = -512;
            }
            cv_beat_current_override = linlin(val, start_cv, 512,
                                              cv_start *
                                                  banks[sel_bank_cur]
                                                      ->sample[sel_sample_cur]
                                                      .snd[FILEZERO]
                                                      ->slice_num /
                                                  1000,
                                              cv_stop *
                                                  banks[sel_bank_cur]
                                                      ->sample[sel_sample_cur]
                                                      .snd[FILEZERO]
                                                      ->slice_num /
                                                  1000);
          } else if (global_amen_cv_behavior == AMEN_CV_BEHAVIOR_SPLIT) {
            if (val < 0) {
              cv_beat_current_override = linlin(val, -512, 0,
                                                cv_start *
                                                    banks[sel_bank_cur]
                                                        ->sample[sel_sample_cur]
                                                        .snd[FILEZERO]
                                                        ->slice_num /
                                                    1000,
                                                cv_stop *
                                                    banks[sel_bank_cur]
                                                        ->sample[sel_sample_cur]
                                                        .snd[FILEZERO]
                                                        ->slice_num /
                                                    1000);
            } else if (val >= 0 && has_changed) {
              cv_beat_current_override = linlin(val, 0, 512,
                                                cv_start *
                                                    banks[sel_bank_cur]
                                                        ->sample[sel_sample_cur]
                                                        .snd[FILEZERO]
                                                        ->slice_num /
                                                    1000,
                                                cv_stop *
                                                    banks[sel_bank_cur]
                                                        ->sample[sel_sample_cur]
                                                        .snd[FILEZERO]
                                                        ->slice_num /
                                                    1000);
            }
          }
        } else if (i == CV_BREAK) {
          // printf("[ectocore] cv_break %d\n", val);
          int16_t cv_min = 0;
          if (global_break_cv_bipolar) {
            cv_min = -512;
          }
          break_set(linlin(val, cv_min, 512, 0, 1024), true, false);
        } else if (i == CV_SAMPLE) {
          // printf("[ectocore] cv_sample %d\n", val);
          int16_t cv_min = 0;
          if (global_sample_cv_bipolar) {
            cv_min = -512;
          }
          sel_sample_next_new =
              linlin(val, cv_min, 512, 0, banks[sel_bank_cur]->num_samples);
          if (debounce_file_change == 0 &&
              sel_sample_cur != sel_sample_next_new) {
            debounce_file_change = DEBOUNCE_FILE_SWITCH;
          }
        }
      }
    }

    uint32_t current_time = to_ms_since_boot(get_absolute_time());

    if (debounce_file_change > 0) {
      if (debounce_file_switching > 0) {
        if (current_time - debounce_file_switching > 100) {
          debounce_file_switching = 0;
        }
      } else {
        debounce_file_change--;
        if (sf->fx_active[FX_TIMESTRETCH]) {
          debounce_file_change = DEBOUNCE_FILE_SWITCH;
        }
        if (debounce_file_change == 0 && fil_current_change == false &&
            (sel_sample_next_new != sel_sample_cur ||
             sel_bank_cur != sel_bank_next_new)) {
          sel_sample_next = sel_sample_next_new;
          sel_bank_next = sel_bank_next_new;
          printf("[ectocore] switch bank/sample %d/%d\n", sel_bank_next,
                 sel_sample_next);
          fil_current_change = true;
          debounce_file_switching = current_time;
        }
      }
    }

    // turn off trigout after 50 ms
    if (ecto_trig_out_last > 0) {
      if (current_time - ecto_trig_out_last > 50) {
        gpio_put(GPIO_TRIG_OUT, 0);
        ecto_trig_out_last = 0;
      }
    }

    // check the clock output if trig mode is active
    if (clock_output_trig && clock_output_trig_time > 0) {
      if (current_time - clock_output_trig_time > 100) {
        gpio_put(GPIO_CLOCK_OUT, 0);
        clock_output_trig_time = 0;
      }
    }

    // check for input
    int char_input = getchar_timeout_us(10);
    if (char_input >= 0) {
      if (char_input == 118) {
        printf("version=v6.2.4\n");
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
    if (!clock_start_stop_sync && clock_in_do) {
      if ((time_us_32() - clock_in_last_time) > 2 * clock_in_diff_2x) {
        clock_in_ready = false;
        clock_in_do = false;
        clock_in_activator = 0;
      }
    }
    if (gpio_btn_taptempo_val == 0 && !btn_taptempo_on) {
      if (clock_input_absent && clock_in_activator >= 3) {
        printf("reseting clock\n");
        clock_in_ready = false;
        clock_in_activator = 0;
        clock_in_do = false;
      } else if (!clock_input_absent) {
        // do nothing (this is handled in TAP+MODE now)
      } else {
        val = TapTempo_tap(taptempo);
        if (val > 0) {
          printf("[ectocore] tap bpm -> %d\n", val);
          sf->bpm_tempo = val;
        }
      }
      btn_taptempo_on = true;
      gpio_put(GPIO_LED_TAPTEMPO, 0);
    } else if (gpio_btn_taptempo_val == 1 && btn_taptempo_on) {
      btn_taptempo_on = false;
      gpio_put(GPIO_LED_TAPTEMPO, 1);
    }

    for (uint8_t i = 0; i < KNOB_NUM; i++) {
      int16_t raw_val = MCP3208_read(mcp3208, knob_gpio[i], false);
      val = KnobChange_update(knob_change[i], raw_val);
      if (debounce_startup == 15 + i) {
        val = raw_val;
        printf("[ectocore] knob %d=%d\n", i, val);
      } else if (i == KNOB_BREAK && cv_was_unplugged[CV_BREAK]) {
        cv_was_unplugged[CV_BREAK] = false;
        val = raw_val;
        printf("[ectocore] (CV_BREAK) knob %d=%d\n", i, val);
      } else if (i == KNOB_AMEN && cv_was_unplugged[CV_AMEN]) {
        cv_was_unplugged[CV_AMEN] = false;
        val = raw_val;
        printf("[ectocore] (CV_AMEN) knob %d=%d\n", i, val);
      } else if (i == KNOB_SAMPLE && cv_was_unplugged[CV_SAMPLE]) {
        cv_was_unplugged[CV_SAMPLE] = false;
        val = raw_val;
        printf("[ectocore] (CV_SAMPLE) knob %d=%d\n", i, val);
      }
      if (val < 0) {
        continue;
      }
      // normalize val based on center calibration
      if (val < sf->center_calibration[knob_gpio[i]]) {
        val = linlin(val, 0, sf->center_calibration[knob_gpio[i]], 0, 512);
      } else {
        val = linlin(val - sf->center_calibration[knob_gpio[i]], 0,
                     1023 - sf->center_calibration[knob_gpio[i]], 0, 512) +
              512;
        if (val > 1023) {
          val = 1023;
        }
      }
      knob_val[i] = val;
      if (knob_gpio[i] == MCP_KNOB_SAMPLE) {
        if (gpio_get(GPIO_BTN_BANK) == 0 && fil_current_change == false) {
          // bank selection
          uint8_t bank_i =
              roundf((float)(val * (banks_with_samples_num - 1)) / 1024.0);
          // printf("[ectocore] switch bank %d\n", val);
          uint8_t bank_num = banks_with_samples[bank_i];
          if (banks[bank_num]->num_samples > 0 &&
              sel_bank_next_new != bank_num && !fil_current_change &&
              !sync_using_sdcard) {
            sel_bank_next_new = bank_num;
            if (sel_bank_next_new != sel_bank_cur) {
              sel_sample_next_new =
                  roundf((float)(sel_sample_cur *
                                 banks[sel_bank_next_new]->num_samples) /
                         banks[sel_bank_cur]->num_samples);
              if (sel_sample_next_new >=
                  banks[sel_bank_next_new]->num_samples) {
                sel_sample_next_new = banks[sel_bank_next_new]->num_samples - 1;
              }
              debounce_file_change = DEBOUNCE_FILE_SWITCH;
              printf("[ectocore] knob switch %d+%d/%d\n", sel_bank_next_new,
                     sel_sample_next_new,
                     banks[sel_bank_next_new]->num_samples);
              ws2812_set_wheel_section(ws2812, bank_i, banks_with_samples_num,
                                       41, 0, 255);
              WS2812_show(ws2812);
            }
          }
        } else {
          if (gpio_btn_taptempo_val == 0) {
            // tap + sample changes gating
            if (val < 128) {
              // gating off
              if (sf->fx_active[FX_TIGHTEN]) {
                printf("[ectocore] gating off\n");
                toggle_fx(FX_TIGHTEN);
              }
            } else {
              // gating on
              if (!sf->fx_active[FX_TIGHTEN]) {
                printf("[ectocore] gating on\n");
                toggle_fx(FX_TIGHTEN);
              }
              // change gating based on the value
              sf->fx_param[FX_TIGHTEN][0] = linlin(val, 128, 1024, 0, 200);
              Gate_set_amount(audio_gate,
                              45 + 200 - sf->fx_param[FX_TIGHTEN][0]);
            }
            ws2812_set_wheel(ws2812, val * 4, 255, 255, 0);
          } else {
            // sample selection
            val = (val * banks[sel_bank_cur]->num_samples) / 1024;
            if (val != sel_sample_next_new) {
              sel_sample_next_new = val;
              ws2812_wheel_clear(ws2812);
              WS2812_fill_color(
                  ws2812, val * 16 / banks[sel_bank_cur]->num_samples, BLUE);
              WS2812_show(ws2812);
              if (sel_sample_next_new != sel_sample_cur) {
                debounce_file_change = DEBOUNCE_FILE_SWITCH;
                printf("[ectocore] knob select sample %d/%d (%d)\n",
                       sel_sample_next_new + 1,
                       banks[sel_bank_cur]->num_samples, raw_val);
              }
            }
          }
        }
      } else if (knob_gpio[i] == MCP_KNOB_BREAK) {
        printf("[ectocore] knob_break %d\n", val);
        break_set(val, false, true);
      } else if (knob_gpio[i] == MCP_KNOB_AMEN) {
        printf("[ectocore] knob_amen %d\n", val);
        if (gpio_btn_taptempo_val == 0) {
          // TODO: change the filter cutoff!
          const uint16_t val_mid = 60;
          if (val < 512 - val_mid) {
            // low pass filter
            global_filter_index =
                val * (resonantfilter_fc_max) / (512 - val_mid);
            // printf("[ectocore] lowpass: %d\n", global_filter_index);
            global_filter_lphp = 0;
            for (uint8_t channel = 0; channel < 2; channel++) {
              ResonantFilter_setFilterType(resFilter[channel],
                                           global_filter_lphp);
              ResonantFilter_setFc(resFilter[channel], global_filter_index);
            }
            ws2812_set_wheel_left_half(ws2812, 2 * val, false, true, true);
          } else if (val > 512 + val_mid) {
            // high pass filter
            global_filter_index = (val - (512 + val_mid)) *
                                  (resonantfilter_fc_max) / (512 - val_mid);
            // printf("[ectocore] highpass: %d\n", global_filter_index);
            global_filter_lphp = 1;
            for (uint8_t channel = 0; channel < 2; channel++) {
              ResonantFilter_setFilterType(resFilter[channel],
                                           global_filter_lphp);
              ResonantFilter_setFc(resFilter[channel], global_filter_index);
            }
            ws2812_set_wheel_right_half(ws2812, 2 * (val - (512 + val_mid)),
                                        true, false, true);
          } else {
            // no filter
            global_filter_index = resonantfilter_fc_max;
            global_filter_lphp = 0;
            for (uint8_t channel = 0; channel < 2; channel++) {
              ResonantFilter_setFilterType(resFilter[channel],
                                           global_filter_lphp);
              ResonantFilter_setFc(resFilter[channel], global_filter_index);
            }
            ws2812_wheel_clear(ws2812);
            WS2812_show(ws2812);
          }
        } else {
          if (!cv_plugged[CV_AMEN]) {
            if (val < 57) {
              for (uint8_t i = 0; i < 16; i++) {
                WS2812_fill(ws2812, i, 123, 32, 12);
              }
              WS2812_show(ws2812);
              // disable random sequence mode
              random_sequence_length = 0;
              do_retrig_at_end_of_phrase = false;
            } else if (val < 966) {
              uint8_t sequence_lengths[12] = {
                  1, 2, 3, 4, 5, 6, 8, 10, 12, 14, 16, 32,
              };
              uint8_t new_random_sequence_length =
                  sequence_lengths[((int16_t)(val - 57) * 12 / (966 - 57)) %
                                   12];
              if (new_random_sequence_length != random_sequence_length) {
                random_sequence_length = new_random_sequence_length;
                ws2812_set_wheel_euclidean(ws2812, random_sequence_length, 123,
                                           32, 12);
                printf("[ectocore] random_sequence_length %d\n",
                       random_sequence_length);
                do_retrig_at_end_of_phrase = false;
              }
            } else {
              printf("[ectocore] regen sequence\n");
              // generative mode + generate new sequence
              for (uint8_t i = 0; i < 64; i++) {
                random_sequence_arr[i] = random_integer_in_range(0, 64);
              }
              debounce_ws2812_set_wheel = debounce_ws2812_set_wheel_time;
              for (uint8_t i = 0; i < 16; i++) {
                uint8_t r, g, b;
                uint16_t i_ = i * 16;
                if (i_ > 255) {
                  i_ = 255;
                }
                hue_to_rgb(i_, &r, &g, &b);
                WS2812_fill(ws2812, i, r, g, b);
              }
              WS2812_show(ws2812);
              random_sequence_length = 8;
              do_retrig_at_end_of_phrase = true;
            }
          } else {
            random_sequence_length = 0;
            do_retrig_at_end_of_phrase = false;
            // show the current offset
            set_cv_start_stop(knob_val[KNOB_AMEN], knob_val[KNOB_AMEN_ATTEN]);
          }
        }
      } else if (knob_gpio[i] == MCP_ATTEN_BREAK) {
        printf("[ectocore] knob_break_atten %d\n", val);
        // change the grimoire rune
        grimoire_rune = val * 7 / 1024;
        // show the current effects toggled for this rune
        ws2812_wheel_clear(ws2812);
        for (uint8_t j = 0; j < 16; j++) {
          if (grimoire_rune_effect[grimoire_rune][j]) {
            WS2812_fill(ws2812, j, 255, 144, 144);
          }
        }
        WS2812_show(ws2812);

      } else if (knob_gpio[i] == MCP_ATTEN_AMEN) {
        printf("[ectocore] knob_amen_atten %d\n", val);
        // check if CV is plugged in for AMEN
        if (!cv_plugged[CV_AMEN]) {
          if (val < 512 - 24) {
            sf->stay_in_sync = false;
            probability_of_random_jump = ((512 - 24) - val) * 100 / (512 - 24);
            ws2812_set_wheel_left_half(ws2812, 2 * val, true, false, true);
          } else if (val > 512 + 24) {
            sf->stay_in_sync = true;
            probability_of_random_jump = (val - (512 + 24)) * 100 / (512 + 24);
            ws2812_set_wheel_right_half(ws2812, 2 * (val - (512 + 24)), true,
                                        false, true);
          } else {
            sf->stay_in_sync = true;
            probability_of_random_jump = 0;
            ws2812_wheel_clear(ws2812);
            WS2812_show(ws2812);
          }
        } else {
          probability_of_random_jump = 0;
          set_cv_start_stop(knob_val[KNOB_AMEN], knob_val[KNOB_AMEN_ATTEN]);
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
      gpio_btn_state[i] = val;
      if (val) {
        gpio_btn_last_pressed[i] = current_time;
      } else {
        gpio_btn_held_time[i] = current_time - gpio_btn_last_pressed[i];
      }
      if (gpio_btns[i] == GPIO_BTN_MODE) {
        printf("[ectocore] btn_mode %d\n", val);
        // check if taptempo button is pressed
        if (!val && gpio_btn_held_time[i] > 2000 && audio_variant_num > 0) {
          // easter egg..toggle lo-fi mode
          if (!mode_amiga) {
            mode_amiga = false;
            set_audio_variant(0);
          } else {
            mode_amiga = true;
            set_audio_variant(audio_variant_num);
          }
        } else if (gpio_btn_state[BTN_TAPTEMPO] == 1) {
          if (val == 1) {
            // TAP + MODE resets to original bpm if no clock is present
            // otherwise it resets the pattern to beat 1
            if (!clock_input_absent) {
              printf("resetting pattern to beat 1\n");
              // pressing clock while clock is active will reset to beat 1
              // after determining whether the press is closer to the last
              // clock or the next clock (i.e. we are either early or late)
              uint32_t next_time =
                  clock_in_last_time +
                  (clock_in_last_time - clock_in_last_last_time);
              uint32_t now_time = time_us_32();
              // ---|-----------|-----------|-------
              // --lastlast----last---NOW--next
              // determine if we are in the first half or the second half
              if (now_time >
                  clock_in_last_time +
                      (clock_in_last_time - clock_in_last_last_time) / 2) {
                // we are in the second half
                // the next clock is going to be the first beat
                // reset it to -1, so that when it increments it will be at 0
                clock_in_beat_total = -1;
              } else {
                // we are in the first half
                // the next clock is going to be the second beat
                // reset it to 0, so that when it increments it will be at 1
                clock_in_beat_total = 0;
              }
            } else {
              // reset tempo to the tempo of the current sample
              int16_t new_tempo = banks[sel_bank_cur]
                                      ->sample[sel_sample_cur]
                                      .snd[FILEZERO]
                                      ->bpm;
              if (new_tempo >= 60 && new_tempo <= 300) {
                sf->bpm_tempo = new_tempo;
              }
            }
          }
        } else {
          if (val == 1) {
            if (ectocore_trigger_mode < 4 - 1) {
              ectocore_trigger_mode++;
            } else {
              ectocore_trigger_mode = 0;
            }
            switch (ectocore_trigger_mode) {
              case TRIGGER_MODE_KICK:
                printf("[ectocore] trigger mode: kick\n");
#ifdef ECTOCORE_VERSION_3
                gpio_put(GPIO_MODE_LEDA, 0);
                gpio_put(GPIO_MODE_LEDB, 0);
#endif
#ifdef ECTOCORE_VERSION_4
                for (uint8_t i = 0; i < 4; i++) {
                  gpio_put(gpio_mode_leds[i], 1);
                }
                gpio_put(gpio_mode_leds[0], 0);
#endif
                break;
              case TRIGGER_MODE_SNARE:
                printf("[ectocore] trigger mode: snare\n");
#ifdef ECTOCORE_VERSION_3
                gpio_put(GPIO_MODE_LEDA, 1);
                gpio_put(GPIO_MODE_LEDB, 0);
#endif
#ifdef ECTOCORE_VERSION_4
                for (uint8_t i = 0; i < 4; i++) {
                  gpio_put(gpio_mode_leds[i], 1);
                }
                gpio_put(gpio_mode_leds[1], 0);
#endif
                break;
              case TRIGGER_MODE_HH:
                printf("[ectocore] trigger mode: hh\n");
#ifdef ECTOCORE_VERSION_3
                gpio_put(GPIO_MODE_LEDA, 0);
                gpio_put(GPIO_MODE_LEDB, 1);
#endif
#ifdef ECTOCORE_VERSION_4
                for (uint8_t i = 0; i < 4; i++) {
                  gpio_put(gpio_mode_leds[i], 1);
                }
                gpio_put(gpio_mode_leds[2], 0);
#endif
                break;
              case TRIGGER_MODE_RANDOM:
                printf("[ectocore] trigger mode: random\n");
#ifdef ECTOCORE_VERSION_3
                gpio_put(GPIO_MODE_LEDA, 1);
                gpio_put(GPIO_MODE_LEDB, 1);
#endif
#ifdef ECTOCORE_VERSION_4
                for (uint8_t i = 0; i < 4; i++) {
                  gpio_put(gpio_mode_leds[i], 1);
                }
                gpio_put(gpio_mode_leds[3], 0);
#endif
                break;
            }
          }
        }
      } else if (gpio_btns[i] == GPIO_BTN_BANK) {
        printf("[ectocore] btn_bank %d\n", val);
        if (val == 0) {
          if (i < BUTTON_NUM && current_time - gpio_btn_last_pressed[i] < 200 &&
              fil_current_change == false) {
            // "tap"
            // switch the bank by one
            if (banks_with_samples_num > 1) {
              uint8_t bank_i = 0;
              for (uint8_t k = 0; k < banks_with_samples_num; k++) {
                if (sel_bank_cur == banks_with_samples[k]) {
                  bank_i = k;
                  break;
                }
              }
              bank_i++;
              if (bank_i >= banks_with_samples_num) {
                bank_i = 0;
              }
              uint8_t bank_num = banks_with_samples[bank_i];
              printf("[ectocore] btn switch bank_num %d\n", bank_num);
              if (banks[bank_num]->num_samples > 0 &&
                  sel_bank_next_new != bank_num) {
                sel_bank_next_new = bank_num;
                if (sel_bank_next_new != sel_bank_cur) {
                  sel_sample_next_new =
                      roundf((float)(sel_sample_cur *
                                     banks[sel_bank_next_new]->num_samples) /
                             banks[sel_bank_cur]->num_samples);
                  if (sel_sample_next_new >=
                      banks[sel_bank_next_new]->num_samples) {
                    sel_sample_next_new =
                        banks[sel_bank_next_new]->num_samples - 1;
                  }
                  debounce_file_change = DEBOUNCE_FILE_SWITCH;
                  printf("[ectocore] btn switch %d+%d/%d\n", sel_bank_next_new,
                         sel_sample_next_new,
                         banks[sel_bank_next_new]->num_samples);
                  ws2812_set_wheel_section(ws2812, bank_i,
                                           banks_with_samples_num, 41, 0, 255);
                  WS2812_show(ws2812);
                }
              }
            }
          }
        } else {
          ws2812_set_wheel_section(ws2812, sel_bank_cur, banks_with_samples_num,
                                   41, 0, 255);
          WS2812_show(ws2812);
        }
      } else if (gpio_btns[i] == GPIO_BTN_MULT) {
        if (val == 1) {
          if (gpio_btn_state[BTN_TAPTEMPO] == 1) {
            // A+C
            if (!playback_stopped && !do_stop_playback) {
              printf("[ectocore] ectocore stop\n");
              if (!button_mute) trigger_button_mute = true;
              do_stop_playback = true;
            } else if (playback_stopped && !do_restart_playback) {
              printf("[ectocore] ectocore start\n");
              cancel_repeating_timer(&timer);
              do_restart_playback = true;
              timer_step();
              add_repeating_timer_us(-(round(30000000 / sf->bpm_tempo / 96)),
                                     repeating_timer_callback, NULL, &timer);
              button_mute = false;
            }
            TapTempo_reset(taptempo);
          } else if (gpio_btn_state[BTN_MODE]) {
#ifdef INCLUDE_CUEDSOUNDS
            // play a random sound
            if (CUEDSOUNDS_FILES >= 4) {
              cuedsounds_do_play = random_integer_in_range(0, 3);
            }
#endif
          } else {
            printf("[ectocore] btn_mult %d %d\n", val,
                   gpio_btn_state[BTN_TAPTEMPO]);
            btn_mult_on_time = current_time;
            btn_mult_hold_time = btn_mult_on_time;
          }
        } else {
          if (gpio_btn_state[BTN_TAPTEMPO]) {
          } else if (gpio_btn_state[BTN_MODE]) {
          } else {
            if (current_time - btn_mult_on_time < 200) {
              // tap
              printf("[ectocore] btn_mult tap\n");
              if (ectocore_clock_selected_division > 0)
                ectocore_clock_selected_division--;
            }
          }
        }
      } else if (gpio_btns[i] == GPIO_BTN_TAPTEMPO) {
        printf("[ectocore] btn_taptempo %d\n", val);
        if (val == 1) {
          if (playback_stopped && !do_restart_playback) {
            cancel_repeating_timer(&timer);
            do_restart_playback = true;
            timer_step();
            add_repeating_timer_us(-(round(30000000 / sf->bpm_tempo / 96)),
                                   repeating_timer_callback, NULL, &timer);
            button_mute = false;
            TapTempo_reset(taptempo);
          }
        }
      }
      // check for reset
      if (gpio_btn_state[BTN_BANK] > 0 && gpio_btn_state[BTN_MODE] > 0 &&
          gpio_btn_state[BTN_MULT] > 0) {
        printf("[ectocore] reset\n");
        // sleep_ms(10);
        watchdog_reboot(0, SRAM_END, 0);
        for (;;) {
          __wfi();
        }
      }
    }

    // check for button mult holding, and change the clock division
    // every second
    if (gpio_btn_state[BTN_MULT] > 0 && gpio_btn_state[BTN_MODE] == 0 &&
        gpio_btn_state[BTN_BANK] == 0 && gpio_btn_state[BTN_TAPTEMPO] == 0) {
      if (current_time - btn_mult_hold_time > 1000) {
        btn_mult_hold_time = current_time;
        // hold
        if (ectocore_clock_selected_division < ECTOCORE_CLOCK_NUM_DIVISIONS - 1)
          ectocore_clock_selected_division++;
      }
    }

    // updating the random fx
    if (debounce_startup == 0) {
      break_fx_update();
    }

    // load the new sample if variation changed
    if (sel_variation_next != sel_variation) {
      bool do_try_change = false;
      if (!audio_callback_in_mute) {
        // uint32_t time_start = time_us_32();
        sleep_us(100);
        while (!sync_using_sdcard) {
          sleep_us(100);
        }
        // printf("sync1: %ld\n", time_us_32() - time_start);
        uint32_t time_start = time_us_32();
        sleep_us(100);
        while (sync_using_sdcard) {
          sleep_us(100);
        }
        // printf("sync2: %ld\n", time_us_32() - time_start);
        // make sure the audio block was faster than usual
        if (time_us_32() - time_start < 4000) {
          do_try_change = true;
        }
      }
      if (do_try_change) {
        sync_using_sdcard = true;
        // measure the time it takes
        uint32_t time_start = time_us_32();
        FRESULT fr = f_close(&fil_current);
        if (fr != FR_OK) {
          printf("[main] f_close error: %s\n", FRESULT_str(fr));
        }
        sprintf(fil_current_name, "bank%d/%d.%d.wav", sel_bank_cur + 1,
                sel_sample_cur, sel_variation_next + audio_variant * 2);
        fr = f_open(&fil_current, fil_current_name, FA_READ);
        if (fr != FR_OK) {
          printf("[main] f_open error: %s\n", FRESULT_str(fr));
        }

        // TODO: fix this
        // if sel_variation_next == 0
        phases[0] = round(((float)phases[0] *
                           (float)sel_variation_scale[sel_variation_next]) /
                          (float)sel_variation_scale[sel_variation]);

        sel_variation = sel_variation_next;
        sync_using_sdcard = false;
        printf("[main] sel_variation %d us\n", time_us_32() - time_start);
      }
    }

    // updating the leds
    if (debounce_ws2812_set_wheel > 0) {
      debounce_ws2812_set_wheel--;
    } else {
      if (Dazzle_update(dazzle, ws2812)) {
        // dazzling
        // always show the current slice
        WS2812_fill_color(ws2812,
                          banks[sel_bank_cur]
                                  ->sample[sel_sample_cur]
                                  .snd[FILEZERO]
                                  ->slice_current %
                              16,
                          CYAN);
        WS2812_show(ws2812);
      } else {
        // highlight the current sample in the leds
        for (uint8_t i = 0; i < 17; i++) {
          WS2812_fill(ws2812, i, 0, 0, 0);
        }
        WS2812_fill_color(ws2812, 16, CYAN);
        WS2812_fill_color(ws2812, 17, CYAN);
        if (retrig_beat_num > 0 && retrig_beat_num % 2 == 0) {
          for (uint8_t i = 0; i < 16; i++) {
            uint8_t r, g, b;
            hue_to_rgb((float)255 / (retrig_beat_num + 1), &r, &g, &b);
            WS2812_fill(ws2812, i, r * led_brightness / 255,
                        g * led_brightness / 255, b * led_brightness / 255);
          }
        }
        if (sf->fx_active[FX_COMB]) {
          for (uint8_t i = 2; i < 14; i += 2) {
            WS2812_fill_color_dim(ws2812,
                                  (banks[sel_bank_cur]
                                       ->sample[sel_sample_cur]
                                       .snd[FILEZERO]
                                       ->slice_current +
                                   i) %
                                      16,
                                  CYAN, 4);
          }
        }
        if (sf->fx_active[FX_TAPE_STOP]) {
          float val = Envelope2_get(envelope_pitch);
          uint8_t r, g, b;
          hue_to_rgb((uint16_t)round(val * 255) % 256, &r, &g, &b);
          for (int8_t i = -1; i < 2; i++) {
            WS2812_fill(ws2812,
                        (banks[sel_bank_cur]
                             ->sample[sel_sample_cur]
                             .snd[FILEZERO]
                             ->slice_current +
                         i) %
                            16,
                        r, g, b);
          }
        }

        if (sf->fx_active[FX_EXPAND]) {
          Dazzle_restart(dazzle, 3);
        }
        // add flourishes if effects are on
        if (sf->fx_active[FX_REVERSE]) {
          WS2812_fill_color_dim(ws2812,
                                (banks[sel_bank_cur]
                                     ->sample[sel_sample_cur]
                                     .snd[FILEZERO]
                                     ->slice_current +
                                 1) %
                                    16,
                                CYAN, 2);
          WS2812_fill_color_dim(ws2812,
                                (banks[sel_bank_cur]
                                     ->sample[sel_sample_cur]
                                     .snd[FILEZERO]
                                     ->slice_current +
                                 2) %
                                    16,
                                CYAN, 3);
          WS2812_fill_color_dim(ws2812,
                                (banks[sel_bank_cur]
                                     ->sample[sel_sample_cur]
                                     .snd[FILEZERO]
                                     ->slice_current +
                                 3) %
                                    16,
                                CYAN, 4);
        }
        if (sf->fx_active[FX_DELAY]) {
          WS2812_fill_color_dim(ws2812,
                                (banks[sel_bank_cur]
                                     ->sample[sel_sample_cur]
                                     .snd[FILEZERO]
                                     ->slice_current +
                                 8) %
                                    16,
                                CYAN, 2);
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

        // always show the current slice
        WS2812_fill_color(ws2812,
                          banks[sel_bank_cur]
                                  ->sample[sel_sample_cur]
                                  .snd[FILEZERO]
                                  ->slice_current %
                              16,
                          CYAN);
        WS2812_show(ws2812);
      }
      sleep_ms(1);
    }
  }
}
