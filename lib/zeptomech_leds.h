

const uint16_t debounce_ws2812_set_wheel_time = 10000;
uint16_t debounce_ws2812_set_wheel = 0;

void ws2812_mode_color(WS2812 *ws2812) {
  // if (mode_amiga) {
  //   WS2812_fill_color(ws2812, 16, YELLOW);
  //   WS2812_fill_color(ws2812, 17, YELLOW);
  // } else {
  //   WS2812_fill_color(ws2812, 16, BLANK);
  //   WS2812_fill_color(ws2812, 17, BLANK);
  // }
}

void ws2812_wheel_clear(WS2812 *ws2812) {
  debounce_ws2812_set_wheel = debounce_ws2812_set_wheel_time;
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    WS2812_fill(ws2812, i, 0, 0, 0);
  }
}

void ws2812_set_wheel_section(WS2812 *ws2812, uint8_t val, uint8_t max,
                              uint8_t r, uint8_t g, uint8_t b) {
  debounce_ws2812_set_wheel = debounce_ws2812_set_wheel_time;
  ws2812_wheel_clear(ws2812);

  static uint8_t n = NUM_LEDS;
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

  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    if (arr[i] == val) {
      WS2812_fill(ws2812, i, r, g, b);
    }
  }
}

void ws2812_set_wheel_euclidean(WS2812 *ws2812, uint8_t val, uint8_t r,
                                uint8_t g, uint8_t b) {
  debounce_ws2812_set_wheel = debounce_ws2812_set_wheel_time;
  ws2812_wheel_clear(ws2812);
  bool rhythm[NUM_LEDS];
  while (val > NUM_LEDS) {
    val = val - NUM_LEDS;
    // // swap r, g, b
    // uint8_t temp = r;
    // r = g;
    // g = b;
    // b = temp;
  }
  uint8_t k = val;
  generate_euclidean_rhythm(NUM_LEDS, k, (NUM_LEDS / k) / 2 + 1, rhythm);
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
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
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
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

  for (uint8_t i = 0; i < NUM_LEDS; i++) {
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
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    WS2812_fill(ws2812, i, 0, 0, 0);
  }

  int8_t filled = (val * NUM_LEDS) / 1024;          // Scale val to range 0-8
  int16_t remainder = (val * NUM_LEDS) % 1024 / 4;  // Scale remainder to range 0-255
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

  int8_t filled = (val * NUM_LEDS/2) / 1024;          // Scale val to range 0-8
  int16_t remainder = (val * NUM_LEDS/2) % 1024 / 4;  // Scale remainder to range 0-255
  if (remainder > 255) {
    remainder = 255;
  }
  for (uint8_t i = 0; i < NUM_LEDS/2; i++) {
    WS2812_fill(ws2812, i, 0, 0, 0);
  }
  for (uint8_t i = 8; i < NUM_LEDS/2 + filled; i++) {
    WS2812_fill(ws2812, i, r ? 255 : 0, g ? 255 : 0, b ? 255 : 0);
  }

  if (remainder < 10) {
    remainder = 0;
  }

  if (filled < 8) {
    WS2812_fill(ws2812, NUM_LEDS/2 + filled, r ? remainder : 0, g ? remainder : 0,
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
  int8_t filled = (val * NUM_LEDS/2) / 1024;          // Scale val to range 0-8
  int16_t remainder = (val * NUM_LEDS/2) % 1024 / 4;  // Scale remainder to range 0-255
  if (remainder > 255) {
    remainder = 255;
  }
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
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

