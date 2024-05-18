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

#ifndef DAZZLE_LIB
#define DAZZLE_LIB 1

#include "WS2812.h"

static inline uint32_t get_current_time_ms(void) {
  return to_ms_since_boot(get_absolute_time());
}

typedef struct Dazzle {
  uint8_t colors[16][3];
  uint32_t last_update_time;
  uint8_t step;
  uint16_t step_limit;
  uint8_t effect_num;
  uint8_t brightness;
  int8_t direction;
} Dazzle;

Dazzle *Dazzle_malloc() {
  Dazzle *self = (Dazzle *)malloc(sizeof(Dazzle));
  self->effect_num = 0;
  self->brightness = 255;
  self->direction = 1;
  self->step_limit = 31;
  return self;
}

void Dazzle_free(Dazzle *self) { free(self); }

void Dazzle_start(Dazzle *self, uint8_t effect_num) {
  self->effect_num = effect_num;
  if (effect_num == 0) {
    uint8_t initial_colors[16][3] = {
        {255, 0, 0},     {255, 127, 0},   {255, 255, 0},   {127, 255, 0},
        {0, 255, 0},     {0, 255, 127},   {0, 255, 255},   {0, 127, 255},
        {0, 0, 255},     {127, 0, 255},   {255, 0, 255},   {255, 0, 127},
        {127, 127, 127}, {255, 255, 255}, {127, 255, 127}, {255, 127, 255}};
    for (uint8_t i = 0; i < 16; ++i) {
      self->colors[i][0] = initial_colors[i][0];
      self->colors[i][1] = initial_colors[i][1];
      self->colors[i][2] = initial_colors[i][2];
    }
    self->brightness = 255;
    self->direction = 1;
    self->step_limit = 31;
  } else if (effect_num == 1) {
    // Initialize colors for breathing effect
    for (uint8_t i = 0; i < 16; ++i) {
      self->colors[i][0] = 255;
      self->colors[i][1] = 255;
      self->colors[i][2] = 255;
    }
    self->brightness = 1;
    self->direction = 31;
    self->step_limit = 40;
  } else if (effect_num == 2) {
    // Initialize colors for rainbow cycle effect
    for (uint8_t i = 0; i < 16; ++i) {
      uint8_t hue = (i * 256 / 16) & 255;
      uint8_t r, g, b;
      if (hue < 85) {
        r = hue * 3;
        g = 255 - hue * 3;
        b = 0;
      } else if (hue < 170) {
        hue -= 85;
        r = 255 - hue * 3;
        g = 0;
        b = hue * 3;
      } else {
        hue -= 170;
        r = 0;
        g = hue * 3;
        b = 255 - hue * 3;
      }
      self->colors[i][0] = r;
      self->colors[i][1] = g;
      self->colors[i][2] = b;
    }
    self->brightness = 255;
    self->direction = 1;
    self->step_limit = 34;  // Increased step limit for smooth cycle
  }
  self->last_update_time = get_current_time_ms();
  self->step = 0;
}

void hue_to_rgb(uint8_t hue, uint8_t *r, uint8_t *g, uint8_t *b) {
  if (hue < 85) {
    *r = hue * 3;
    *g = 255 - hue * 3;
    *b = 0;
  } else if (hue < 170) {
    hue -= 85;
    *r = 255 - hue * 3;
    *g = 0;
    *b = hue * 3;
  } else {
    hue -= 170;
    *r = 0;
    *g = hue * 3;
    *b = 255 - hue * 3;
  }
}

bool Dazzle_update(Dazzle *self, WS2812 *ws2812) {
  uint32_t current_time = get_current_time_ms();
  if (self->step < self->step_limit) {
    for (uint8_t i = 0; i < 16; ++i) {
      WS2812_fill(ws2812, i, self->colors[i][0] * self->brightness / 255,
                  self->colors[i][1] * self->brightness / 255,
                  self->colors[i][2] * self->brightness / 255);
    }
    WS2812_show(ws2812);
    if (current_time - self->last_update_time >= 16) {
      if (self->effect_num == 0) {
        // Shift colors to the right
        uint8_t temp_r = self->colors[15][0];
        uint8_t temp_g = self->colors[15][1];
        uint8_t temp_b = self->colors[15][2];
        for (uint8_t i = 15; i > 0; --i) {
          self->colors[i][0] = self->colors[i - 1][0];
          self->colors[i][1] = self->colors[i - 1][1];
          self->colors[i][2] = self->colors[i - 1][2];
        }
        self->colors[0][0] = temp_r;
        self->colors[0][1] = temp_g;
        self->colors[0][2] = temp_b;
      } else if (self->effect_num == 1) {
        if ((int8_t)(self->brightness) + self->direction >= 255 ||
            (int8_t)(self->brightness) + self->direction <= 0) {
          self->direction = -self->direction;

          // Change to a new random color when fading out completely
          if (self->direction > 0) {
            uint8_t hue = rand() % 256;
            for (uint8_t i = 0; i < 16; ++i) {
              hue_to_rgb(hue, &self->colors[i][0], &self->colors[i][1],
                         &self->colors[i][2]);
            }
          }
        }
        self->brightness += self->direction;
      } else if (self->effect_num == 2) {
        // Cycle through the rainbow
        uint8_t new_hue = (self->step * 2 * 256 / self->step_limit) & 255;
        for (uint8_t i = 0; i < 16; ++i) {
          uint8_t hue = (new_hue + (i * 256 / 16)) & 255;
          hue_to_rgb(hue, &self->colors[i][0], &self->colors[i][1],
                     &self->colors[i][2]);
        }
      }

      self->last_update_time = current_time;
      self->step++;
    }
    return true;
  } else {
    return false;
  }
}

#endif /* DAZZLE_LIB */
