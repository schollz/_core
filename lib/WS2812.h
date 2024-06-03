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

#ifndef WS2812_H
#define WS2812_H

#include "WS2812.pio.h"

#define NUM_LEDS 18

const uint8_t ws2812_brightness_values[16] = {
    0, 1, 2, 3, 4, 6, 10, 15, 21, 30, 39, 51, 64, 80, 97, 117,
    // disallow brightness > 50% of the RGB
    // 140, 164, 192, 222, 255,
};

typedef struct WS2812 {
  uint pin;
  PIO pio;
  uint sm;
  uint8_t bytes[4];
  uint32_t data[NUM_LEDS];
  uint8_t brightness;
} WS2812;

WS2812 *WS2812_new(uint pin, PIO pio, uint sm) {
  WS2812 *ws;
  ws = malloc(sizeof(WS2812));
  ws->pin = pin;
  ws->pio = pio;
  ws->sm = sm;
  ws->brightness = 255;
  memset(ws->data, 0, sizeof(ws->data));
  ws->bytes[0] = 0;
  ws->bytes[1] = 2;
  ws->bytes[2] = 1;
  ws->bytes[3] = 3;
  uint offset = pio_add_program(pio, &ws2812_program);
  uint bits = 24;
  ws2812_program_init(pio, sm, offset, pin, 800000, bits);
  return ws;
}

void WS2812_set_brightness(WS2812 *ws, uint8_t brightness) {
  // brightness between 0 and 100
  if (brightness > 100) {
    brightness = 100;
  }
  ws->brightness = ws2812_brightness_values[brightness * 16 / 100];
  if (brightness > 0 && ws->brightness == 0) {
    ws->brightness = 1;
  }
  return;
}

void WS2812_fill(WS2812 *ws, int index, uint8_t red, uint8_t green,
                 uint8_t blue) {
  if (index < 0 || index >= NUM_LEDS) {
    return;  // Safety check to avoid overflow
  }
  // scale by brightness level
  red = (red * ws->brightness) / 255;
  green = (green * ws->brightness) / 255;
  blue = (blue * ws->brightness) / 255;
  uint32_t rgbw =
      (uint32_t)(blue) << 16 | (uint32_t)(green) << 8 | (uint32_t)(red);
  uint32_t result = 0;
  for (uint b = 0; b < 4; b++) {
    switch (ws->bytes[b]) {
      case 1:
        result |= (rgbw & 0xFF);
        break;
      case 2:
        result |= (rgbw & 0xFF00) >> 8;
        break;
      case 3:
        result |= (rgbw & 0xFF0000) >> 16;
        break;
      case 4:
        result |= (rgbw & 0xFF000000) >> 24;
        break;
    }
    result <<= 8;
  }
  ws->data[index] = result;  // Store data for the specified LED
  return;
}

void WS2812_show(WS2812 *ws) {
  for (int i = 0; i < NUM_LEDS; i++) {
    pio_sm_put_blocking(ws->pio, ws->sm, ws->data[i]);
  }
  return;
}

#endif