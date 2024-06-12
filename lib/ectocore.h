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
uint16_t break_knob_set_point = 0;

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

uint8_t break_fx_beat_refractory_min_max[32] = {
    4,  16,  // distortion
    4,  16,  // loss
    4,  16,  // bitcrush
    4,  16,  // filter
    16, 32,  // time stretch
    4,  16,  // delay
    4,  16,  // comb
    4,  8,   // beat repeat
    2,  16,  // reverb
    2,  6,   // autopan
    8,  32,  // pitch down
    8,  32,  // pitch up
    1,  8,   // reverse
    4,  8,   // retrigger no pitch
    8,  16,  // retrigger w/ pitch
    32, 64,  // tapestop
};
uint8_t break_fx_beat_duration_min_max[32] = {
    2, 4,   // distortion
    2, 4,   // loss
    2, 4,   // bitcrush
    4, 8,   // filter
    4, 32,  // time stretch
    4, 16,  // delay
    2, 6,   // comb
    1, 4,   // beat repeat
    4, 12,  // reverb
    4, 8,   // autopan
    8, 32,  // pitch down
    8, 32,  // pitch up
    1, 8,   // reverse
    4, 8,   // retrigger no pitch
    4, 12,  // retrigger w/ pitch
    8, 16,  // tape stop
};
uint8_t break_fx_probability_scaling[16] = {
    50,  // distortion
    50,  // loss
    50,  // bitcrush
    50,  // filter
    50,  // time stretch
    80,  // delay
    50,  // comb
    40,  // beat repeat
    50,  // reverb
    50,  // autopan
    40,  // pitch down
    30,  // pitch up
    70,  // reverse
    50,  // retirgger no pitch
    30,  // retrigger with pitch,
    5,   // tape sotp
};

uint8_t break_fx_beat_activated[16] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};
uint8_t break_fx_beat_after_activated[16] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

// if (random_integer_in_range(1, 2000000) < probability_of_random_retrig) {
//   printf("[ecotocre] random retrigger\n");
//   sf->do_retrig_pitch_changes = (random_integer_in_range(1, 10) < 5);
//   go_retrigger_2key(random_integer_in_range(0, 15),
//                     random_integer_in_range(0, 15));
// }

void do_do_retrigger(uint8_t effect, bool on, bool pitch_chanes) {
  // retrigger no pitch
  if (on && retrig_beat_num == 0) {
    sf->do_retrig_pitch_changes = pitch_chanes;
    debounce_quantize = 0;
    retrig_first = true;
    retrig_beat_num = break_fx_beat_activated[effect];
    uint8_t retrig_timer_dividers[6] = {8, 6, 4, 3, 2, 1};
    uint8_t divider = retrig_timer_dividers[random_integer_in_range(0, 5)];
    retrig_timer_reset = 96 / divider;
    retrig_beat_num = retrig_beat_num * divider;
    if (divider > 4) {
      retrig_beat_num = retrig_beat_num / 2;
    }
    retrig_vol = 0.02;
    retrig_vol_step = ((float)random_integer_in_range(15, 50) / 100.0) /
                      ((float)retrig_beat_num);
    if (random_integer_in_range(0, 100) < 50) {
      beat_current = beat_current_last;
    }
    retrig_ready = true;
  } else if (!on) {
    retrig_beat_num = 0;
    retrig_ready = false;
    retrig_vol = 1.0;
    retrig_pitch = PITCH_VAL_MID;
  }
}

void break_fx_toggle(uint8_t effect, bool on) {
  // if (effect != 4) {
  //   return;
  // }
  if (on) {
    // set the activation time
    break_fx_beat_activated[effect] =
        random_integer_in_range(break_fx_beat_duration_min_max[effect * 2],
                                break_fx_beat_duration_min_max[effect * 2 + 1]);
    printf("[break_fx_toggle] fx %d on for %d beats\n", effect + 1,
           break_fx_beat_activated[effect]);
  } else {
    // set the refractory period
    break_fx_beat_after_activated[effect] = random_integer_in_range(
        break_fx_beat_refractory_min_max[effect * 2],
        break_fx_beat_refractory_min_max[effect * 2 + 1]);
    printf("[break_fx_toggle] fx %d off for %d beats\n", effect + 1,
           break_fx_beat_after_activated[effect]);
  }

  switch (effect) {
    case 0:
      // distortion
      if (on) {
        sf->fx_active[FX_FUZZ] = true;
      } else {
        sf->fx_active[FX_FUZZ] = false;
      }
      update_fx(FX_FUZZ);
      break;
    case 1:
      // loss
      if (on) {
        sf->fx_param[FX_SHAPER][0] = random_integer_in_range(0, 255);
        sf->fx_param[FX_SHAPER][1] = random_integer_in_range(0, 255);
        sf->fx_active[FX_SHAPER] = true;
      } else {
        sf->fx_active[FX_SHAPER] = false;
      }
      update_fx(FX_SHAPER);
      break;
    case 2:
      // bitcrush
      if (on) {
        sf->fx_param[FX_BITCRUSH][0] = random_integer_in_range(220, 255);
        sf->fx_param[FX_BITCRUSH][1] = random_integer_in_range(210, 255);
        sf->fx_active[FX_BITCRUSH] = true;
      } else {
        sf->fx_active[FX_BITCRUSH] = false;
      }
      update_fx(FX_BITCRUSH);
      break;
    case 3:
      // filter
      if (on) {
        sf->fx_param[FX_FILTER][0] = random_integer_in_range(0, 128);
        sf->fx_param[FX_FILTER][1] = random_integer_in_range(0, 64);
        sf->fx_active[FX_FILTER] = true;
      } else {
        sf->fx_active[FX_FILTER] = false;
      }
      update_fx(FX_FILTER);
      break;
    case 4:
      // time stretch
      if (on) {
        sf->fx_active[FX_TIMESTRETCH] = true;
      } else {
        sf->fx_active[FX_TIMESTRETCH] = false;
      }
      update_fx(FX_TIMESTRETCH);
      break;
    case 5:
      // time-synced delay
      if (on) {
        uint8_t faster = 1;
        if (random_integer_in_range(0, 100) < 25) {
          faster = 2;
        }
        if (sf->bpm_tempo > 140) {
          Delay_setDuration(delay, (30 * 44100) / sf->bpm_tempo / faster);
        } else {
          Delay_setDuration(delay, (15 * 44100) / sf->bpm_tempo / faster);
        }
        uint8_t feedback = random_integer_in_range(0, 4);
        if (feedback == 0 && break_fx_beat_activated[effect] > 6) {
          feedback = 1;
        }
        Delay_setFeedback(delay, feedback);
        sf->fx_active[FX_DELAY] = true;
      } else {
        sf->fx_active[FX_DELAY] = false;
      }
      update_fx(FX_DELAY);
      break;
    case 6:
      // combo
      if (on) {
        sf->fx_param[FX_COMB][0] = random_integer_in_range(0, 255);
        sf->fx_param[FX_COMB][1] = random_integer_in_range(0, 255);
        sf->fx_active[FX_COMB] = true;
      } else {
        sf->fx_active[FX_COMB] = false;
      }
      update_fx(FX_COMB);
      break;
    case 7:
      // beat repeat
      sf->fx_active[FX_BEATREPEAT] = on;
      update_fx(FX_BEATREPEAT);
      break;
    case 8:
      // reverb
      sf->fx_active[FX_EXPAND] = on;
      update_fx(FX_EXPAND);
      break;
    case 9:
      // autopan
      sf->fx_active[FX_PAN] = on;
      uint8_t possible_speeds[3] = {2, 4, 8};
      lfo_pan_step =
          Q16_16_2PI / (48 * possible_speeds[random_integer_in_range(0, 2)]);
      update_fx(FX_PAN);
      break;
    case 10:
      // pitch down
      if (!sf->fx_active[FX_REPITCH]) {
        sf->fx_param[FX_REPITCH][0] = 0;
        sf->fx_param[FX_REPITCH][1] = random_integer_in_range(0, 100);
      }
      sf->fx_active[FX_REPITCH] = on;
      update_fx(FX_REPITCH);
      break;
    case 11:
      // pitch up
      if (!sf->fx_active[FX_REPITCH]) {
        sf->fx_param[FX_REPITCH][0] = 255;
        sf->fx_param[FX_REPITCH][1] = random_integer_in_range(0, 100);
      }
      sf->fx_active[FX_REPITCH] = on;
      update_fx(FX_REPITCH);
      break;
    case 12:
      // reverse
      sf->fx_active[FX_REVERSE] = on;
      update_fx(FX_REVERSE);
      break;
    case 13:
      // retrigger
      do_do_retrigger(effect, on, false);
      break;
    case 14:
      // retrigger pitched
      do_do_retrigger(effect, on, true);
      break;
    case 15:
      sf->fx_param[FX_TAPE_STOP][0] = random_integer_in_range(0, 128);
      sf->fx_param[FX_TAPE_STOP][1] = random_integer_in_range(0, 128);
      sf->fx_active[FX_TAPE_STOP] = on;
      update_fx(FX_TAPE_STOP);
      break;
    default:
      break;
  }
}

void break_fx_update() {
  if (!beat_did_activate) {
    return;
  }
  beat_did_activate = false;
  uint16_t break_knob_set_point_scaled =
      (((break_knob_set_point * break_knob_set_point) / 1024) *
       break_knob_set_point) /
      1024;
  for (uint8_t effect = 0; effect < 16; effect++) {
    // check if the fx is allowed in the grimoire runes
    if (grimoire_rune_effect[grimoire_rune][effect] == false &&
        break_fx_beat_activated[effect] > 0) {
      // turn off if it is activated
      break_fx_beat_activated[effect] = 0;
      break_fx_toggle(effect, false);
      continue;
    }
    if (break_fx_beat_activated[effect] > 0) {
      break_fx_beat_activated[effect]--;
      if (break_fx_beat_activated[effect] == 0) {
        // turn off the fx
        break_fx_toggle(effect, false);
      }
    } else if (break_fx_beat_after_activated[effect] > 0) {
      // don't allow to be turned on in this refractory period
      break_fx_beat_after_activated[effect]--;
    } else if (grimoire_rune_effect[grimoire_rune][effect] == true) {
      // roll a die to see if the fx is activated
      if (random_integer_in_range(0, 200) <
          break_knob_set_point_scaled * break_fx_probability_scaling[effect] /
              1024) {
        // activate the effect
        break_fx_toggle(effect, true);
      }
    }
  }
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
      sf->fx_param[FX_SATURATE][0] = 100 + (val - 532) * 128 / (768 - 532);
    } else if (val > 768) {
      sf->vol = VOLUME_STEPS;
      sf->fx_active[FX_SATURATE] = 0;
      sf->fx_active[FX_FUZZ] = 1;
      sf->fx_param[FX_FUZZ][0] = (val - 768) * 128 / (1024 - 768) + 127;
    } else {
      sf->vol = VOLUME_STEPS;
      sf->fx_active[FX_FUZZ] = 0;
      sf->fx_active[FX_SATURATE] = 0;
    }
    return true;
  }
  break_knob_set_point = val;
  if (show_wheel) {
    ws2812_set_wheel(ws2812, val * 4, false, false, true);
  }
}

void dust_1() {
  // printf("[ectocore] dust_1\n");
}

void input_handling() {
#ifdef INCLUDE_CUEDSOUNDS
  cuedsounds_do_play = random_integer_in_range(0, 100);
#endif

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

  // update the knobs
#define KNOB_NUM 5
  uint8_t knob_gpio[KNOB_NUM] = {
      MCP_KNOB_BREAK, MCP_ATTEN_BREAK, MCP_KNOB_AMEN,
      MCP_ATTEN_AMEN, MCP_KNOB_SAMPLE,
  };
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

  uint16_t debounce_startup = 8000;
  uint32_t btn_mult_on_time = 0;
  uint32_t btn_mult_hold_time = 0;

  for (uint8_t i = 0; i < 64; i++) {
    random_sequence_arr[i] = random_integer_in_range(0, 64);
  }

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
            watchdog_reboot(0, SRAM_END, 900);
            sleep_ms(10);
            write_struct_to_flash(&write_data, sizeof(write_data));
            sleep_ms(1000);
            for (;;) {
              __wfi();
            }
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
          if (fil_current_change != true && debounce_file_change == 0 &&
              !sf->fx_active[FX_TIMESTRETCH]) {
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

    // turn off trigout after 10 ms
    if (ecto_trig_out_last > 0) {
      if (to_ms_since_boot(get_absolute_time()) - ecto_trig_out_last > 10) {
        gpio_put(GPIO_TRIG_OUT, 0);
        ecto_trig_out_last = 0;
      }
    }

    // check the clock output if trig mode is active
    if (clock_output_trig && clock_output_trig_time > 0) {
      if (to_ms_since_boot(get_absolute_time()) - clock_output_trig_time >
          100) {
        gpio_put(GPIO_CLOCK_OUT, 0);
        clock_output_trig_time = 0;
      }
    }

    // check for input
    int char_input = getchar_timeout_us(10);
    if (char_input >= 0) {
      if (char_input == 118) {
        printf("version=v2.9.1\n");
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
      if (knob_gpio[i] == MCP_KNOB_SAMPLE) {
        if (gpio_get(GPIO_BTN_BANK) == 0 && !sf->fx_active[FX_TIMESTRETCH] &&
            fil_current_change == false) {
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
          } else if (!sf->fx_active[FX_TIMESTRETCH] &&
                     fil_current_change == false) {
            // sample selection
            printf("[ectocore] sample %d\n", val);
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
        }
      } else if (knob_gpio[i] == MCP_KNOB_BREAK) {
        printf("[ectocore] knob_break %d\n", val);
        break_set(val, false, true);
      } else if (knob_gpio[i] == MCP_KNOB_AMEN) {
        printf("[ectocore] knob_amen %d\n", val);
        if (gpio_btn_taptempo_val == 0) {
          // TODO: change the filter cutoff!
          const uint16_t val_mid = 12;
          if (val < 512 - val_mid) {
            // low pass filter
            global_filter_index =
                val * (resonantfilter_fc_max) / (512 - val_mid);
            printf("[ectocore] lowpass: %d\n", global_filter_index);
            for (uint8_t channel = 0; channel < 2; channel++) {
              ResonantFilter_setFilterType(resFilter[channel], 0);
              ResonantFilter_setFc(resFilter[channel], global_filter_index);
            }
            ws2812_set_wheel_left_half(ws2812, 2 * val, false, true, true);
          } else if (val > 512 + val_mid) {
            // high pass filter
            global_filter_index = (val - (512 + val_mid)) *
                                  (resonantfilter_fc_max) / (512 - val_mid);
            printf("[ectocore] highpass: %d\n", global_filter_index);
            for (uint8_t channel = 0; channel < 2; channel++) {
              ResonantFilter_setFilterType(resFilter[channel], 1);
              ResonantFilter_setFc(resFilter[channel], global_filter_index);
            }
            ws2812_set_wheel_right_half(ws2812, 2 * (val - (512 + val_mid)),
                                        true, false, true);
          } else {
            // no filter
            global_filter_index = resonantfilter_fc_max;
            for (uint8_t channel = 0; channel < 2; channel++) {
              ResonantFilter_setFilterType(resFilter[channel], 0);
              ResonantFilter_setFc(resFilter[channel], global_filter_index);
            }
            ws2812_wheel_clear(ws2812);
            WS2812_show(ws2812);
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
                  200 &&
              !sf->fx_active[FX_TIMESTRETCH] && fil_current_change == false) {
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
        } else {
          ws2812_wheel_clear(ws2812);
          WS2812_fill(ws2812, sel_bank_cur, 255, 0, 0);
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
              do_restart_playback = true;
              button_mute = false;
            }
            TapTempo_reset(taptempo);
          } else {
            printf("[ectocore] btn_mult %d %d\n", val,
                   gpio_btn_state[BTN_TAPTEMPO]);
            btn_mult_on_time = to_ms_since_boot(get_absolute_time());
            btn_mult_hold_time = btn_mult_on_time;
          }
        } else {
          if (gpio_btn_state[BTN_TAPTEMPO]) {
          } else {
            if (to_ms_since_boot(get_absolute_time()) - btn_mult_on_time <
                200) {
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
            do_restart_playback = true;
            button_mute = false;
            TapTempo_reset(taptempo);
          }
        }
      }
      // check for reset
      if (gpio_btn_state[BTN_BANK] > 0 && gpio_btn_state[BTN_MODE] > 0 &&
          gpio_btn_state[BTN_MULT] > 0) {
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
      if (to_ms_since_boot(get_absolute_time()) - btn_mult_hold_time > 1000) {
        btn_mult_hold_time = to_ms_since_boot(get_absolute_time());
        // hold
        if (ectocore_clock_selected_division < ECTOCORE_CLOCK_NUM_DIVISIONS - 1)
          ectocore_clock_selected_division++;
      }
    }

    // updating the random fx
    break_fx_update();

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
        phases[0] = round(((float)phases[0] *
                           (float)sel_variation_scale[sel_variation_next]) /
                          (float)sel_variation_scale[sel_variation]);

        sel_variation = sel_variation_next;
        sync_using_sdcard = false;
        printf("[zeptocore] sel_variation %d us\n", time_us_32() - time_start);
      }
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