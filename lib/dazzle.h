// Copyright 2023-2025 Zack Scholl, GPLv3.0

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
  bool still_going;
} Dazzle;

Dazzle *Dazzle_malloc() {
  Dazzle *self = (Dazzle *)malloc(sizeof(Dazzle));
  self->effect_num = 0;
  self->brightness = 255;
  self->direction = 1;
  self->step_limit = 31;
  self->still_going = false;
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
  } else if (effect_num == 3) {
    // Initialize colors for blue/cyan/green wave effect
    for (uint8_t i = 0; i < 16; ++i) {
      if (i % 3 == 0) {
        self->colors[i][0] = 0;  // Blue
        self->colors[i][1] = 0;
        self->colors[i][2] = 255;
      } else if (i % 3 == 1) {
        self->colors[i][0] = 0;  // Cyan
        self->colors[i][1] = 255;
        self->colors[i][2] = 255;
      } else {
        self->colors[i][0] = 0;  // Green
        self->colors[i][1] = 255;
        self->colors[i][2] = 0;
      }
    }
    self->brightness = 255;
    self->direction = 1;
    self->step_limit = 100;  // Adjust the step limit for smooth transitions
  }
  self->last_update_time = get_current_time_ms();
  self->step = 0;
}

void Dazzle_restart(Dazzle *self, uint8_t effect_num) {
  if (!self->still_going) {
    Dazzle_start(self, effect_num);
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
      } else if (self->effect_num == 3) {
        // Smooth sine wave for crossfading and moving blue/cyan/green wave
        // effect
        for (uint8_t i = 0; i < 16; ++i) {
          float wave =
              (sinf(2 * M_PI * (self->step + i) / self->step_limit) + 1) / 2;
          uint8_t blue_intensity =
              (uint8_t)(255 * (1 - wave));                  // Blue fades out
          uint8_t green_intensity = (uint8_t)(255 * wave);  // Green fades in
          uint8_t cyan_intensity =
              (uint8_t)(255 *
                        fabs(
                            sinf(M_PI * (self->step + i) /
                                 self->step_limit)));  // Cyan reaches peak at
                                                       // the middle of the wave

          // Apply crossfade: blending between blue, cyan, and green
          self->colors[i][0] = 0;  // No red component

          // Calculate blended green, cyan, and blue values for smooth
          // transition
          self->colors[i][1] =
              green_intensity;  // Green channel modulated by the wave
          self->colors[i][2] =
              blue_intensity +
              cyan_intensity;  // Blue and Cyan share the blue channel, cyan is
                               // a blend of blue and green
        }

        // Update the WS2812 LED strip with the new color values
        for (uint8_t i = 0; i < 16; ++i) {
          WS2812_fill(ws2812, i, self->colors[i][0], self->colors[i][1],
                      self->colors[i][2]);
        }

        // Slow down the update rate for smooth undulation and movement
        if (current_time - self->last_update_time >= 32) {
          self->last_update_time = current_time;
          self->step++;  // Move to the next step of the wave
        }

        // Reset the step for continuous looping
        if (self->step >= self->step_limit) {
          self->step = 0;
        }
      }
      self->last_update_time = current_time;
      self->step++;
    }
    self->still_going = true;
  } else {
    self->still_going = false;
  }
  return self->still_going;
}

#endif /* DAZZLE_LIB */
