// Copyright 2023-2025 Zack Scholl, GPLv3.0

#include "clockhandling.h"
//
#include "break_knob.h"
#include "mcp3208.h"
#include "midicallback.h"
#include "onewiremidi2.h"
#ifdef INCLUDE_MIDI
#include "midi_comm_callback.h"
#endif

#define KNOB_ATTEN_ZERO_WIDTH 80
#define DEBOUNCE_FILE_SWITCH 500

// Input detection configuration
#define SIGNAL_SETTLE_TIME_US 8  // Time for signal to settle
#define SIGNAL_READ_SAMPLES 1    // Number of samples to average
#define DETECTION_THRESHOLD 5    // Consecutive matches needed for state change
#define SIGNAL_THRESHOLD_MARGIN 50  // ADC counts above/below mean for detection
#define MEAN_ALPHA 0.1f             // EMA coefficient for mean signal
#define DETECTION_INTERVAL_MS 10    // Minimum time between detection runs
#define MEAN_SIGNAL_INTERVAL_MS 1000  // Time between mean signal recalculations
#define SIGNAL_ERROR_TOLERANCE 2      // Allow N bit errors in pattern matching

uint8_t gpio_btn_taptempo_val = 0;
#ifdef ECTOCORE_VERSION_4
static const uint8_t gpio_mode_leds[4] = {
    GPIO_MODE_1,
    GPIO_MODE_2,
    GPIO_MODE_3,
    GPIO_MODE_4,
};
#endif

// toggle the fx
void toggle_fx(uint8_t fx_num) {
  sf->fx_active[fx_num] = !sf->fx_active[fx_num];
  update_fx(fx_num);
}

const uint16_t debounce_ws2812_set_wheel_time = 10000;
uint16_t debounce_ws2812_set_wheel = 0;

void ws2812_mode_color(WS2812 *ws2812) {
  if (dual_leds_holding_mode) {
    WS2812_fill_color(ws2812, 16, YELLOW);
    WS2812_fill_color(ws2812, 17, YELLOW);
  } else if (dual_leds_holding_tap) {
    WS2812_fill_color(ws2812, 16, CYAN);
    WS2812_fill_color(ws2812, 17, CYAN);
  } else {
    WS2812_fill_color(ws2812, 16, BLANK);
    WS2812_fill_color(ws2812, 17, BLANK);
  }
}

void update_gpios_for_mode() {
  switch (ectocore_trigger_mode) {
    case TRIGGER_MODE_KICK:
      // printf("[ectocore] trigger mode: kick\n");
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
      // printf("[ectocore] trigger mode: snare\n");
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
    // // swap r, g, b
    // uint8_t temp = r;
    // r = g;
    // g = b;
    // b = temp;
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
    // printf("[ectocore] cv_start=%d, cv_stop=%d\n", cv_start, cv_stop);
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
  ws2812_mode_color(ws2812);
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
    fuzz_auto_active = false;
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
    if (sf->fx_active[FX_FUZZ]) {
      fuzz_manual_lock = true;
    } else {
      fuzz_manual_lock = false;
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

bool clock_input_absent = true;

// CV monitoring feature (bank+mode button toggle)
bool cv_monitor_active = false;
uint32_t cv_monitor_start_time = 0;
uint32_t cv_monitor_last_print = 0;
#define CV_MONITOR_INTERVAL_MS 100
#define CV_MONITOR_DURATION_MS 60000

ClockInput *clockinput;
void gpio_callback(uint gpio, uint32_t events) {
  if (gpio != GPIO_CLOCK_IN) return;
  bool clock_up = events & 4;
  if (cv_reset_override == CV_CLOCK) {
    // check GPIO_CLOCK_IN
    if (clock_up) {
      if (!cv_reset_override_active) {
        cv_reset_override_active = true;
        timer_reset();
        // printf("[ectocore] cv_reset_override %d\n",
        // cv_reset_override_active);
      }
    } else {
      if (cv_reset_override_active) {
        cv_reset_override_active = false;
        // printf("[ectocore] cv_reset_override %d\n",
        // cv_reset_override_active);
      }
    }
  } else {
    if (clock_up && clock_input_absent) {
      clock_input_present_first = true;
    }
    ClockInput_update_raw(clockinput, clock_up);
  }
}

bool dont_wait = false;
void __not_in_flash_func(input_handling)() {
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

  uint32_t last_input_detection_time = 0;
  uint32_t last_mean_signal_time = 0;
  float mean_signal_ema = 0;
  const uint8_t length_signal = 16;
  // Improved magic signals with better Hamming distance
  uint8_t magic_signal[3][16] = {
      {0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0},
      {1, 0, 1, 1, 0, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1},
      {0, 0, 1, 0, 1, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 1},
  };
  bool cv_was_unplugged[3] = {false, false, false};
  uint8_t cv_detection_count[3] = {0, 0, 0};
  uint16_t detection_errors[3] = {0, 0, 0};
  uint8_t signal_strength[3] = {0, 0, 0};

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
    knob_change[i] = KnobChange_malloc(12);
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

  // // create clock/midi
  // Onewiremidi *onewiremidi =
  //     Onewiremidi_new(pio0, 3, GPIO_MIDI_IN, midi_note_on, midi_note_off,
  //                     midi_start, midi_continue, midi_stop, midi_timing);
  clockinput = ClockInput_create(GPIO_CLOCK_IN, clock_handling_up,
                                 clock_handling_down, clock_handling_start);
  gpio_set_irq_enabled_with_callback(GPIO_CLOCK_IN,
                                     GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
                                     true, &gpio_callback);

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

  uint16_t debounce_startup = 400;
  if (do_calibration_mode) {
    debounce_startup = 110;
  }
  uint32_t btn_mult_on_time = 0;
  uint32_t btn_mult_hold_time = 0;
  uint32_t debounce_file_switching = 0;
  uint8_t sel_bank_next_new = sel_bank_cur;
  uint8_t sel_sample_next_new = sel_sample_cur;

  regenerate_random_sequence_arr();

#ifdef INCLUDE_CUEDSOUNDS
  mute_soft = true;
#endif

  int cv_amen_last_value = 0;
  uint8_t knob_selector = 0;

  while (1) {
#ifdef INCLUDE_MIDI
    tud_task();
    midi_comm_task(midi_comm_callback_fn, NULL, NULL, NULL, NULL, NULL, NULL,
                   NULL);
#endif
    int16_t val;
    if (debounce_startup > 0) {
      debounce_startup--;
      if (debounce_startup == 10) {
        // printf("clock_start_stop_sync: %d\n", clock_start_stop_sync);
      } else if (debounce_startup == 9) {
        // printf("global_brightness: %d\n", global_brightness);
      } else if (debounce_startup == 7) {
#ifdef INCLUDE_CUEDSOUNDS
        cuedsounds_do_play = random_integer_in_range(0, CUEDSOUNDS_FILES - 1);
#endif
        // reset probabilities
        probability_of_random_jump = 0;
      } else if (debounce_startup == 108) {
        printf("[ectocore] startup\n");
        // read flash data
        uint16_t calibration_data[8];
        if (PersistentState_load_calibration(calibration_data)) {
          for (uint8_t i = 0; i < 8; i++) {
            sf->center_calibration[i] = calibration_data[i];
          }
          printf("[ectocore] calibration data loaded from flash\n");
        } else {
          printf(
              "[ectocore] calibration data is corrupted or missing, using "
              "defaults\n");
        }
      } else if (debounce_startup >= 100 && debounce_startup < 108) {
        uint8_t i = debounce_startup - 100;
        if (do_calibration_mode) {
          sleep_ms(1);
          sf->center_calibration[i] = MCP3208_read(mcp3208, i, false);
          if (i == 0) {
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
            PersistentState_save_calibration(sf->center_calibration);
            sleep_ms(3000);
            for (;;) {
              __wfi();
            }
          }
        }
        printf("[ectocore] calibrate %d=%d,", i, sf->center_calibration[i]);
      }
    }

    // update dusts
    for (uint8_t i = 0; i < DUST_NUM; i++) {
      Dust_update(dust[i]);
    }

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

    // Calculate mean signal using exponential moving average
    uint32_t current_time = to_ms_since_boot(get_absolute_time());
    if (current_time - last_mean_signal_time >= MEAN_SIGNAL_INTERVAL_MS) {
      int16_t total_mean_signal = 0;
      uint8_t total_signals_sent = 0;
      for (uint8_t j = 0; j < 3; j++) {
        if (!cv_plugged[j]) {
          total_signals_sent++;
          for (uint8_t i = 0; i < length_signal; i++) {
            gpio_put(GPIO_INPUTDETECT, magic_signal[j][i]);
            sleep_us(SIGNAL_SETTLE_TIME_US);
            total_mean_signal += MCP3208_read(mcp3208, cv_signals[j], false);
          }
        }
      }
      if (total_signals_sent > 0) {
        float new_sample =
            (float)total_mean_signal / (total_signals_sent * length_signal);
        if (mean_signal_ema == 0) {
          mean_signal_ema = new_sample;  // Initialize on first run
        } else {
          mean_signal_ema =
              MEAN_ALPHA * new_sample + (1.0f - MEAN_ALPHA) * mean_signal_ema;
        }
        // printf("[ectocore] mean_signal_ema: %f\n", mean_signal_ema);
      }
      last_mean_signal_time = current_time;
    }

    // Input detection with time-based debouncing
    if (mean_signal_ema > 0 &&
        current_time - last_input_detection_time >= DETECTION_INTERVAL_MS) {
      int16_t val_input;
      uint8_t response_signal[3][16] = {
          {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
          {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
          {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      };

      for (uint8_t j = 0; j < 3; j++) {
        int16_t total_signal_strength = 0;
        for (uint8_t i = 0; i < length_signal; i++) {
          gpio_put(GPIO_INPUTDETECT, magic_signal[j][i]);
          sleep_us(SIGNAL_SETTLE_TIME_US);
          val_input = MCP3208_read(mcp3208, cv_signals[j], false);

          // Track signal strength for diagnostics
          int16_t signal_diff = val_input - (int16_t)mean_signal_ema;
          total_signal_strength += abs(signal_diff);

          // Threshold with margin to reduce noise sensitivity
          if (val_input > (int16_t)mean_signal_ema + SIGNAL_THRESHOLD_MARGIN) {
            response_signal[j][i] = 1;
          } else if (val_input <
                     (int16_t)mean_signal_ema - SIGNAL_THRESHOLD_MARGIN) {
            response_signal[j][i] = 0;
          } else {
            // Ambiguous reading - use previous state or default to 0
            response_signal[j][i] = 0;
          }
        }
        // Store average signal strength
        signal_strength[j] = total_signal_strength / length_signal;
      }

      // Validate signals with error correction
      bool is_signal[3] = {true, true, true};
      for (uint8_t j = 0; j < 3; j++) {
        uint8_t count_matches = 0;
        for (uint8_t i = 0; i < length_signal; i++) {
          if (response_signal[j][i] == magic_signal[j][i]) {
            count_matches++;
          }
        }
        // Allow SIGNAL_ERROR_TOLERANCE bit errors
        is_signal[j] =
            (count_matches >= length_signal - SIGNAL_ERROR_TOLERANCE);
        if (!is_signal[j] &&
            count_matches < length_signal - SIGNAL_ERROR_TOLERANCE) {
          detection_errors[j]++;
        }
      }

      // Hysteresis-based state machine
      for (uint8_t j = 0; j < 3; j++) {
        if (!is_signal[j] && !cv_plugged[j]) {
          // Potential plug-in detected
          cv_detection_count[j]++;
          if (cv_detection_count[j] >= DETECTION_THRESHOLD) {
            cv_plugged[j] = true;
            cv_detection_count[j] = 0;
            last_mean_signal_time = 0;  // Trigger mean recalculation
            printf("[ectocore] cv_%d plugged\n", j);
          }
        } else if (is_signal[j] && cv_plugged[j]) {
          // Potential unplug detected
          cv_detection_count[j]++;
          if (cv_detection_count[j] >= DETECTION_THRESHOLD) {
            cv_plugged[j] = false;
            cv_detection_count[j] = 0;
            cv_was_unplugged[j] = true;
            last_mean_signal_time = 0;  // Trigger mean recalculation
            printf("[ectocore] cv_%d unplugged\n", j);
          }
        } else {
          // State matches expectation - reset counter
          cv_detection_count[j] = 0;
        }
      }

      last_input_detection_time = current_time;
    }

    if (fuzz_manual_lock && !sf->fx_active[FX_FUZZ]) {
      sf->fx_active[FX_FUZZ] = true;
    }

    // update the cv for each channel
    // Ensure GPIO_INPUTDETECT is LOW to prevent race condition
    gpio_put(GPIO_INPUTDETECT, 0);
    sleep_us(SIGNAL_SETTLE_TIME_US);

    for (uint8_t i = 0; i < 3; i++) {
      if (cv_plugged[i]) {
        // collect out CV values
        val = MCP3208_read(mcp3208, cv_signals[i], false) - 512;
        if (cv_reset_override == i) {
          if (cv_reset_override_active && val < 128) {
            cv_reset_override_active = false;
          } else if (!cv_reset_override_active && val > 250) {
            cv_reset_override_active = true;
            // check if clock is active
            if (clock_input_absent && clock_in_activator >= 0) {
              timer_reset();
            } else {
              clock_handling_start();
            }
          }
          continue;
        }
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
          if (sel_sample_cur != sel_sample_next_new) {
            debounce_file_change = 1;
            dont_wait = true;
          }
        }
      }
    }

    current_time = to_ms_since_boot(get_absolute_time());

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
          // printf("[ectocore] switch bank/sample %d/%d\n", sel_bank_next,
          //        sel_sample_next);
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
        printf("version=v7.0.1\n");
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
      if (clock_in_do && clock_input_absent && clock_in_activator >= 0) {
        clock_in_ready = false;
        clock_in_activator = 0;
        clock_in_do = false;
        timer_reset();
      } else if (!clock_input_absent) {
        // do nothing (this is handled in TAP+MODE now)
      } else {
        val = TapTempo_tap(taptempo);
        if (val > 0) {
          sf->bpm_tempo = val;
        }
      }
      btn_taptempo_on = true;
      gpio_put(GPIO_LED_TAPTEMPO, 0);
    } else if (gpio_btn_taptempo_val == 1 && btn_taptempo_on) {
      btn_taptempo_on = false;
      gpio_put(GPIO_LED_TAPTEMPO, 1);
    }

    knob_selector++;
    if (knob_selector >= KNOB_NUM) {
      knob_selector = 0;
    }
    for (uint8_t i = 0; i < KNOB_NUM; i++) {
      if (i != knob_selector) {
        continue;
      }
      int16_t raw_val = MCP3208_read(mcp3208, knob_gpio[i], false);
      val = KnobChange_update(knob_change[i], raw_val);
      if (debounce_startup == 15 + i) {
        val = raw_val;
        // printf("[ectocore] knob %d=%d\n", i, val);
      } else if (i == KNOB_BREAK && cv_was_unplugged[CV_BREAK]) {
        cv_was_unplugged[CV_BREAK] = false;
        val = raw_val;
        // printf("[ectocore] (CV_BREAK) knob %d=%d\n", i, val);
      } else if (i == KNOB_AMEN && cv_was_unplugged[CV_AMEN]) {
        cv_was_unplugged[CV_AMEN] = false;
        val = raw_val;
        // printf("[ectocore] (CV_AMEN) knob %d=%d\n", i, val);
      } else if (i == KNOB_SAMPLE && cv_was_unplugged[CV_SAMPLE]) {
        cv_was_unplugged[CV_SAMPLE] = false;
        val = raw_val;
        // printf("[ectocore] (CV_SAMPLE) knob %d=%d\n", i, val);
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
      // printf("[ectocore] knob %d=%d\n", i, val);
      knob_val[i] = val;
      if (knob_gpio[i] == MCP_KNOB_SAMPLE) {
        if (mode_held_duration > MODE_HOLD_DURATION_THRESHOLD) {
          // mode selection
          // 0 - 100
          mode_chaos_trembler = val * 100 / 1024;
          printf("[ectocore] mode_chaos_trembler %d\n", mode_chaos_trembler);
          ws2812_set_wheel(ws2812, val * 4, 255, 0, 0);
        } else if (gpio_get(GPIO_BTN_BANK) == 0 &&
                   fil_current_change == false) {
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
              // printf("[ectocore] knob switch %d+%d/%d\n", sel_bank_next_new,
              //        sel_sample_next_new,
              //        banks[sel_bank_next_new]->num_samples);
              ws2812_set_wheel_section(ws2812, bank_i, banks_with_samples_num,
                                       41, 0, 255);
              WS2812_show(ws2812);
            }
          }
        } else {
          if (mode_held_duration > MODE_HOLD_DURATION_THRESHOLD) {
          } else if (gpio_btn_taptempo_val == 0) {
            // tap + sample changes gating
            if (val < 128) {
              // gating off
              if (sf->fx_active[FX_TIGHTEN]) {
                // printf("[ectocore] gating off\n");
                toggle_fx(FX_TIGHTEN);
              }
            } else {
              // gating on
              if (!sf->fx_active[FX_TIGHTEN]) {
                // printf("[ectocore] gating on\n");
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
                // printf("[ectocore] knob select sample %d/%d (%d)\n",
                //        sel_sample_next_new + 1,
                //        banks[sel_bank_cur]->num_samples, raw_val);
              }
            }
          }
        }
      } else if (knob_gpio[i] == MCP_KNOB_BREAK) {
        // printf("[ectocore] knob_break %d\n", val);
        if (mode_held_duration > MODE_HOLD_DURATION_THRESHOLD) {
          // mode_break_index setting (0 to 20)
          mode_digital_saturation = val * 100 / 1024;
          printf("[ectocore] mode_digital_saturation %d\n",
                 mode_digital_saturation);
          ws2812_set_wheel(ws2812, val * 4, 255, 0, 0);
        } else {
          break_set(val, false, true);
        }
      } else if (knob_gpio[i] == MCP_KNOB_AMEN) {
        // printf("[ectocore] knob_amen %d\n", val);
        if (mode_held_duration > MODE_HOLD_DURATION_THRESHOLD) {
          // mode_amiga_index setting (0 to 20)
          mode_amiga_index = val * 37 / 1024;
          printf("[ectocore] amiga mode %d\n", mode_amiga_index);
          ws2812_set_wheel(ws2812, val * 4, 255, 0, 0);
        } else if (gpio_btn_taptempo_val == 0) {
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
          if (!cv_plugged[CV_AMEN] ||
              (cv_plugged[CV_AMEN] && cv_reset_override == CV_AMEN)) {
            if (val < 57) {
              for (uint8_t i = 0; i < 16; i++) {
                WS2812_fill(ws2812, i, 250, 0, 140);
              }
              WS2812_show(ws2812);
              // disable random sequence mode
              random_sequence_length = 0;
              do_retrig_at_end_of_phrase = false;
            } else if (val < 966) {
              uint8_t sequence_lengths[15] = {
                  1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 14, 16, 24, 32,
              };
              uint8_t new_random_sequence_length =
                  sequence_lengths[((int16_t)(val - 57) * 15 / (966 - 57)) %
                                   15];
              if (new_random_sequence_length != random_sequence_length) {
                random_sequence_length = new_random_sequence_length;
                if (random_sequence_length % 2 == 0) {
                  ws2812_set_wheel_euclidean(ws2812, random_sequence_length,
                                             255, 0, 140);
                } else {
                  ws2812_set_wheel_euclidean(ws2812, random_sequence_length,
                                             255, 150, 30);
                }
                // printf("[ectocore] random_sequence_length %d\n",
                //        random_sequence_length);
                do_retrig_at_end_of_phrase = false;
              }
            } else {
              // printf("[ectocore] regen sequence\n");
              // generative mode + generate new sequence
              regenerate_random_sequence_arr();
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
        if (mode_held_duration > MODE_HOLD_DURATION_THRESHOLD) {
          // mode_digital_depth setting (0 to 100)
          mode_digital_smear = val * 100 / 1024;
          printf("[ectocore] mode_digital_smear %d\n", mode_digital_smear);
          ws2812_set_wheel(ws2812, val * 4, 255, 0, 0);
        } else if (gpio_btn_taptempo_val == 0) {
          planned_retrig_probability = val * 100 / 1024;
          ws2812_set_wheel(ws2812, val * 4, 0, 0, 255);
        } else {
          // printf("[ectocore] knob_break_atten %d\n", val);
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
        }

      } else if (knob_gpio[i] == MCP_ATTEN_AMEN) {
        // printf("[ectocore] knob_amen_atten %d\n", val);
        // check if CV is plugged in for AMEN
        if (mode_held_duration > MODE_HOLD_DURATION_THRESHOLD) {
          // mode_amiga_depth setting (0 to 100)
          mode_digital_jitter = val * 100 / 1024;
          printf("[ectocore] mode_digital_jitter %d\n", mode_digital_jitter);
          ws2812_set_wheel(ws2812, val * 4, 255, 0, 0);
        } else if (gpio_btn_taptempo_val == 0) {
          if (val < 512 - 24) {
            // left half: pitch down
            sf->pitch_val_index =
                PITCH_VAL_MID -
                (uint8_t)((512 - 24 - val) * PITCH_VAL_MID / (512 - 24));
            ws2812_set_wheel_left_half(ws2812, 2 * val, false, false, true);
          } else if (val > 512 + 24) {
            // right half: pitch up
            sf->pitch_val_index =
                PITCH_VAL_MID +
                (uint8_t)((val - (512 + 24)) * (PITCH_VAL_MAX - PITCH_VAL_MID) /
                          (512 - 24));
            ws2812_set_wheel_right_half(ws2812, 2 * (val - (512 + 24)), false,
                                        false, true);
          } else {
            sf->pitch_val_index = PITCH_VAL_MID;
            ws2812_wheel_clear(ws2812);
            WS2812_show(ws2812);
          }

        } else if (!cv_plugged[CV_AMEN] ||
                   (cv_plugged[CV_AMEN] && cv_reset_override == CV_AMEN)) {
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
      // ButtonChange_update(button_change[i], gpio_get(gpio_btns[i]));
      // if (val < 0) {
      //   continue;
      // }
      val = gpio_get(gpio_btns[i]);
      val = 1 - val;
      // check for mode hold duration
      if (gpio_btns[i] == GPIO_BTN_MODE && mode_held_duration != 0) {
        uint32_t mode_held_new_duration = current_time - mode_held_start_time;
        if (mode_held_new_duration >= MODE_HOLD_DURATION_THRESHOLD &&
            mode_held_duration < MODE_HOLD_DURATION_THRESHOLD) {
          printf("[ectocore] MODE held for 2 seconds\n");
          if (ectocore_trigger_mode > 0) {
            ectocore_trigger_mode--;
          } else {
            ectocore_trigger_mode = 4 - 1;
          }
          update_gpios_for_mode();
        }
        mode_held_duration = mode_held_new_duration;
      }
#ifdef INCLUDE_EZEPTOCORE
      dual_leds_holding_tap = gpio_btn_state[BTN_TAPTEMPO] == 1;
      dual_leds_holding_mode =
          (mode_held_duration >= MODE_HOLD_DURATION_THRESHOLD);
#endif
      if (val == gpio_btn_state[i]) {
        continue;
      }
      gpio_btn_state[i] = val;
      if (val) {
        gpio_btn_last_pressed[i] = current_time;
      } else {
        gpio_btn_held_time[i] = current_time - gpio_btn_last_pressed[i];
      }
      // // reset all knobchange debouncers
      // for (uint8_t j = 0; j < KNOB_NUM; j++) {
      //   KnobChange_reset(knob_change[j]);
      // }
      if (gpio_btns[i] == GPIO_BTN_MODE) {
        // printf("[ectocore] btn_mode %d\n", val);
        // check if taptempo button is pressed
        if (val && mode_held_duration == 0) {
#ifdef INCLUDE_EZEPTOCORE
          // start counting hold time
          mode_held_start_time = current_time;
          mode_held_duration = 1;
#endif
        } else if (!val && mode_held_duration != 0) {
          // reset hold time
          mode_held_duration = 0;
        }
        if (gpio_btn_state[BTN_TAPTEMPO] == 1) {
          if (val == 1) {
            // TAP + MODE resets to original bpm if no clock is present
            // otherwise it resets the pattern to beat 1
            if (!clock_input_absent) {
              // printf("resetting pattern to beat 1\n");
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
                clock_in_beat_last = -1;
              } else {
                // we are in the first half
                // the next clock is going to be the second beat
                // reset it to 0, so that when it increments it will be at 1
                clock_in_beat_total = 0;
                clock_in_beat_last = 0;
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
            update_gpios_for_mode();
          }
        }
      } else if (gpio_btns[i] == GPIO_BTN_BANK) {
        // printf("[ectocore] btn_bank %d\n", val);
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
              // printf("[ectocore] btn switch bank_num %d\n", bank_num);
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
                  // printf("[ectocore] btn switch %d+%d/%d\n",
                  // sel_bank_next_new,
                  //        sel_sample_next_new,
                  //        banks[sel_bank_next_new]->num_samples);
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
        // printf("[ectocore] btn_mult %d\n", val);
        if (val == 1) {
          if (gpio_btn_state[BTN_TAPTEMPO] == 1) {
            // A+C
            if (!playback_stopped && !do_stop_playback) {
              // printf("[ectocore] ectocore stop\n");
              if (!button_mute) trigger_button_mute = true;
              do_stop_playback = true;
            } else if (playback_stopped && !do_restart_playback) {
              // printf("[ectocore] ectocore start\n");
              timer_reset();
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
            // printf("[ectocore] btn_mult %d %d\n", val,
            //        gpio_btn_state[BTN_TAPTEMPO]);
            btn_mult_on_time = current_time;
            btn_mult_hold_time = btn_mult_on_time;
          }
        } else {
          if (gpio_btn_state[BTN_TAPTEMPO]) {
          } else if (gpio_btn_state[BTN_MODE]) {
          } else {
            if (current_time - btn_mult_on_time < 200) {
              // tap
              // printf("[ectocore] btn_mult tap\n");
              if (ectocore_clock_selected_division > 0)
                ectocore_clock_selected_division--;
            }
          }
        }
      } else if (gpio_btns[i] == GPIO_BTN_TAPTEMPO) {
        // printf("[ectocore] btn_taptempo %d\n", val);
        if (val == 1) {
          if (playback_stopped && !do_restart_playback) {
            cancel_repeating_timer(&timer);
            do_restart_playback = true;
            timer_step();
            update_repeating_timer_to_bpm(sf->bpm_tempo);
            button_mute = false;
            TapTempo_reset(taptempo);
          }
        }
      }
      // check for CV monitor toggle (bank+mode, but not mult)
      if (val == 1 && gpio_btn_state[BTN_BANK] > 0 &&
          gpio_btn_state[BTN_MODE] > 0 && gpio_btn_state[BTN_MULT] == 0 &&
          gpio_btn_state[BTN_TAPTEMPO] == 0) {
        if (gpio_btns[i] == GPIO_BTN_BANK || gpio_btns[i] == GPIO_BTN_MODE) {
          // Toggle CV monitoring feature
          cv_monitor_active = !cv_monitor_active;
          if (cv_monitor_active) {
            cv_monitor_start_time = current_time;
            cv_monitor_last_print = 0;
            printf("[ectocore] CV monitor enabled\n");
          } else {
            printf("[ectocore] CV monitor disabled\n");
          }
        }
      }
      // check for reset
      if (gpio_btn_state[BTN_BANK] > 0 && gpio_btn_state[BTN_MODE] > 0 &&
          gpio_btn_state[BTN_MULT] > 0) {
        // printf("[ectocore] reset\n");
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
      if (dont_wait) {
        do_try_change = true;
        dont_wait = false;
      }
      if (!audio_callback_in_mute && !do_try_change) {
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
        // printf("[main] sel_variation %d us\n", time_us_32() - time_start);
      }
    }

    // CV monitoring - print raw CV values every 100ms for 1 minute
    if (cv_monitor_active) {
      uint32_t elapsed = current_time - cv_monitor_start_time;

      // Check if 1 minute has elapsed
      if (elapsed >= CV_MONITOR_DURATION_MS) {
        cv_monitor_active = false;
        printf("[ectocore] CV monitor disabled (timeout)\n");
      } else if (current_time - cv_monitor_last_print >=
                 CV_MONITOR_INTERVAL_MS) {
        // Read raw CV values
        int16_t cv_amen = MCP3208_read(mcp3208, MCP_CV_AMEN, false);
        int16_t cv_break = MCP3208_read(mcp3208, MCP_CV_BREAK, false);
        int16_t cv_clock = gpio_get(GPIO_CLOCK_IN);  // Digital read for clock
        int16_t cv_sample = MCP3208_read(mcp3208, MCP_CV_SAMPLE, false);

        // Print in requested format: cv1=x,cv2=x,cv3=x,cv4=x
        printf("cv1=%d,cv2=%d,cv3=%d,cv4=%d\n", cv_amen, cv_break, cv_clock,
               cv_sample);

        cv_monitor_last_print = current_time;
      }
    }

    // Check for planned retrig activation on slice change
    // Check on any slice, schedule to start on next slice
    {
      static uint8_t last_slice_for_planned_retrig = 255;
      static uint8_t last_boundary_start = 255;
      static uint8_t last_boundary_end = 255;
      static uint8_t planned_retrig_target_slice = 255;
      static uint8_t planned_retrig_start_slice = 255;
      static bool planned_retrig_pending = false;
      static float pending_start_vol = 0;
      static int8_t pending_start_pitch = 0;
      static float pending_end_vol = PLANNED_RETRIG_USE_CURRENT_VOL;
      static int8_t pending_end_pitch = PLANNED_RETRIG_USE_CURRENT_PITCH;
      static uint8_t pending_beats_remaining = 0;
      static uint8_t pending_times = 0;
      static uint8_t pending_rate_divisor = 1;

      uint8_t current_slice = banks[sel_bank_cur]
                                  ->sample[sel_sample_cur]
                                  .snd[FILEZERO]
                                  ->slice_current;
      uint8_t slice_num =
          banks[sel_bank_cur]->sample[sel_sample_cur].snd[FILEZERO]->slice_num;

      if (current_slice != last_slice_for_planned_retrig) {
        last_slice_for_planned_retrig = current_slice;

        // If we have a pending retrig and hit the start slice, start it
        if (planned_retrig_pending &&
            current_slice == planned_retrig_start_slice &&
            !planned_retrig_ready) {
          planned_retrig_do(pending_start_vol, pending_start_pitch,
                            pending_beats_remaining, pending_times,
                            pending_rate_divisor, pending_end_vol,
                            pending_end_pitch);
          planned_retrig_pending = false;
        }

        // Only check for new retrig when not already active/pending
        if (!planned_retrig_ready && !planned_retrig_pending && slice_num > 1) {
          // Calculate position of next slice in phrase (retrig starts here)
          uint8_t next_slice = (current_slice + 1) % slice_num;
          // Fixed 4-step boundary for planning
          uint8_t boundary_interval = 4;

          // Boundary is defined by the next slice (where retrig starts)
          uint8_t boundary_start =
              (next_slice / boundary_interval) * boundary_interval;
          uint8_t boundary_end = boundary_start + boundary_interval;
          if (boundary_end > slice_num) {
            boundary_end = slice_num;
          }

          // Choose a target slice once per boundary for uniform chance
          if (boundary_start != last_boundary_start ||
              boundary_end != last_boundary_end) {
            last_boundary_start = boundary_start;
            last_boundary_end = boundary_end;
            planned_retrig_target_slice = 255;

            if (random_integer_in_range(0, 100) < planned_retrig_probability) {
              uint8_t first_step = boundary_start;
              uint8_t last_step =
                  (boundary_end > 0) ? (uint8_t)(boundary_end - 1) : 0;
              if (first_step <= last_step) {
                uint8_t step_count = (last_step - first_step) + 1;
                uint8_t step_index =
                    (uint8_t)random_integer_in_range(0, step_count - 1);
                planned_retrig_target_slice = first_step + step_index;
              }
            }
          }

          if (current_slice == planned_retrig_target_slice) {
            // Stutter for a random duration, extend to end on a 4-step boundary
            uint8_t duration_steps = (uint8_t)random_integer_in_range(1, 16);
            uint8_t end_unaligned = next_slice + duration_steps;
            uint8_t end_aligned = ((end_unaligned + 3) / 4) * 4;
            if (end_aligned > slice_num) {
              end_aligned = slice_num;
            }

            uint8_t stutter_slices = end_aligned - next_slice;
            if (stutter_slices < 1) {
              stutter_slices = 1;
            }

            // Select times first (removed 18 - doesn't divide 96 evenly)
            uint8_t times_options[8] = {1, 2, 3, 4, 6, 8, 12, 16};
            pending_times = times_options[random_integer_in_range(0, 7)];
            uint8_t rate_divisor_options[4] = {1, 2, 4, 8};
            pending_rate_divisor =
                rate_divisor_options[random_integer_in_range(0, 3)];

            // Beats remaining until next boundary (each slice is an 8th note)
            uint8_t beats_remaining = stutter_slices / 2;
            if (beats_remaining < 1) {
              beats_remaining = 1;
            }
            pending_beats_remaining =
                (beats_remaining * pending_times) / pending_rate_divisor;
            if (pending_beats_remaining < 1) {
              pending_beats_remaining = 1;
            }

            // Randomize parameters and store for next slice
            // Allowed start modes:
            // 1) normal
            // 2) low pitch -> normal
            // 3) high pitch -> normal
            // 4) volume 0
            // 5) volume 0 + low pitch
            // 6) volume 0 + high pitch
            // 7) normal volume -> volume 0
            // 8) normal volume -> volume 0 + low pitch
            // 9) normal volume -> volume 0 + high pitch
            // 10) normal pitch -> low pitch
            // 11) volume 0 + normal pitch -> low pitch
            // 12) volume 0 + normal pitch -> high pitch
            // 13) normal volume + normal pitch -> low pitch
            // 14) normal volume + normal pitch -> high pitch
            uint8_t mode_roll = random_integer_in_range(0, 13);
            float random_vol = (float)random_integer_in_range(0, 100) / 100.0f;
            pending_end_vol = PLANNED_RETRIG_USE_CURRENT_VOL;
            pending_end_pitch = PLANNED_RETRIG_USE_CURRENT_PITCH;

            if (mode_roll == 0) {
              pending_start_vol = random_vol;
              pending_start_pitch = 0;
            } else if (mode_roll == 1) {
              pending_start_vol = random_vol;
              pending_start_pitch = -24;
            } else if (mode_roll == 2) {
              pending_start_vol = random_vol;
              pending_start_pitch = 24;
            } else if (mode_roll == 3) {
              pending_start_vol = 0.0f;
              pending_start_pitch = 0;
            } else if (mode_roll == 4) {
              pending_start_vol = 0.0f;
              pending_start_pitch = -24;
            } else if (mode_roll == 5) {
              pending_start_vol = 0.0f;
              pending_start_pitch = 24;
            } else if (mode_roll == 6) {
              pending_start_vol = 1.0f;
              pending_start_pitch = 0;
              pending_end_vol = 0.0f;
            } else if (mode_roll == 7) {
              pending_start_vol = 1.0f;
              pending_start_pitch = 0;
              pending_end_vol = 0.0f;
              pending_end_pitch = -24;
            } else if (mode_roll == 8) {
              pending_start_vol = 1.0f;
              pending_start_pitch = 0;
              pending_end_vol = 0.0f;
              pending_end_pitch = 24;
            } else if (mode_roll == 9) {
              pending_start_vol = 1.0f;
              pending_start_pitch = 0;
              pending_end_pitch = -24;
            } else if (mode_roll == 10) {
              pending_start_vol = 0.0f;
              pending_start_pitch = 0;
              pending_end_pitch = -24;
            } else if (mode_roll == 11) {
              pending_start_vol = 0.0f;
              pending_start_pitch = 0;
              pending_end_pitch = 24;
            } else if (mode_roll == 12) {
              pending_start_vol = random_vol;
              pending_start_pitch = 0;
              pending_end_pitch = -24;
            } else {
              pending_start_vol = random_vol;
              pending_start_pitch = 0;
              pending_end_pitch = 24;
            }

            planned_retrig_pending = true;
            planned_retrig_start_slice = next_slice;
          }
        }
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
        ws2812_mode_color(ws2812);
        WS2812_show(ws2812);
      } else {
        // highlight the current sample in the leds
        for (uint8_t i = 0; i < 17; i++) {
          WS2812_fill(ws2812, i, 0, 0, 0);
        }
        ws2812_mode_color(ws2812);
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
