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

#ifndef LIB_LEDS
#define LIB_LEDS 1

#include "definitions.h"
#include "pca9552.h"

#define LEDS_ROWS 5
#define LEDS_COLS 4

typedef struct LEDS {
  // state, 0=off, 1=dim, 2=bright, 3=blink
  // matrix 1 = which face
  // matrix 2 = current beat
  // matrix 3 = button presses
  // matrix 4 = final
  // state 0 = off
  // state 1 = dim (PCA9552 only)
  // state 2 = bright
  // state 3 = blink
  uint8_t state[LEDS_ROWS][LEDS_COLS];
  PCA9552 *pca;
  uint8_t gpio_leds_state[4];
  uint16_t gpio_leds_count[4];
  int8_t gpio_leds_pin[4];
} LEDS;

LEDS *LEDS_create() {
  LEDS *leds = (LEDS *)malloc(sizeof(LEDS));
  for (uint8_t i = 0; i < LEDS_ROWS; i++) {
    for (uint8_t j = 0; j < LEDS_COLS; j++) {
      leds->state[i][j] = 0;
    }
  }
  for (uint8_t i = 0; i < 4; i++) {
    leds->gpio_leds_count[i] = 0;
    leds->gpio_leds_state[i] = 0;
    leds->gpio_leds_pin[i] = -1;
  }

#ifdef LED_1_GPIO
  leds->gpio_leds_pin[0] = LED_1_GPIO;
#endif
#ifdef LED_2_GPIO
  leds->gpio_leds_pin[1] = LED_2_GPIO;
#endif
#ifdef LED_3_GPIO
  leds->gpio_leds_pin[2] = LED_3_GPIO;
#endif
#ifdef LED_4_GPIO
  leds->gpio_leds_pin[3] = LED_4_GPIO;
#endif

  // setup PCA9552 leds
  i2c_init(i2c_default, 40 * 1000);
  gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
  gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
  gpio_pull_up(I2C_SDA_PIN);
  gpio_pull_up(I2C_SCL_PIN);

  // my unique PCA9552 wiring requires a unique mapping
  uint8_t row_map[] = {// row 1
                       0, 0, 3, 3,
                       // row 2
                       0, 0, 3, 3,
                       // row 3
                       1, 1, 2, 2,
                       // row 4
                       1, 1, 2, 2};
  uint8_t col_map[] = {//
                       3, 2, 0, 1,
                       //
                       1, 0, 2, 3,
                       //
                       3, 2, 0, 1,
                       //
                       1, 0, 2, 3};

  leds->pca = PCA9552_create(0x60, i2c_default, row_map, col_map);
  if (leds->pca->error != PCA9552_OK) {
    printf("PCA9552_ERROR: %02x\n", leds->pca->error);
  }
  sleep_ms(1);
  PCA9552_clear(leds->pca);
  PCA9552_render(leds->pca);

  // setup GPIO leds
  for (uint8_t i = 0; i < 4; i++) {
    if (leds->gpio_leds_pin[i] == -1) {
      continue;
    }
    gpio_init(leds->gpio_leds_pin[i]);
    gpio_set_dir(leds->gpio_leds_pin[i], GPIO_OUT);
    gpio_put(leds->gpio_leds_pin[i], 0);
  }

  return leds;
}

void LEDS_set(LEDS *leds, uint8_t led, uint8_t state) {
  // printf("leds->state[%d][%d][%d]=%d\n", face, led / 4, led % 4, state);
  leds->state[led / 4][led % 4] = state;
}

void LEDS_clear(LEDS *leds) {
  for (uint8_t i = 0; i < LEDS_ROWS; i++) {
    for (uint8_t j = 0; j < LEDS_COLS; j++) {
      leds->state[i][j] = 0;
    }
  }
}

void LEDS_render(LEDS *leds) {
  // light up the PCA9552
  for (uint8_t i = 1; i < LEDS_ROWS; i++) {
    for (uint8_t j = 0; j < LEDS_COLS; j++) {
      PCA9552_ledSet(leds->pca, (i - 1) * 4 + j, leds->state[i][j]);
    }
  }
  PCA9552_render(leds->pca);

  // light up the GPIO leds
  for (uint8_t j = 0; j < 4; j++) {
    if (leds->gpio_leds_state[j] != leds->state[0][j]) {
      leds->gpio_leds_state[j] = leds->state[0][j];
      if (leds->gpio_leds_state[j] == LED_NONE) {
        gpio_put(leds->gpio_leds_pin[j], 0);
      } else if (leds->gpio_leds_state[j] == LED_BRIGHT) {
        gpio_put(leds->gpio_leds_pin[j], 1);
      }
    }
    // blink GPIO leds
    if (leds->gpio_leds_state[j] == LED_BLINK) {
      leds->gpio_leds_count[j]++;
      if (leds->gpio_leds_count[j] == 700) {
        gpio_put(leds->gpio_leds_pin[j], 1);
      } else if (leds->gpio_leds_count[j] >= 1400) {
        gpio_put(leds->gpio_leds_pin[j], 0);
        leds->gpio_leds_count[j] = 0;
      }
    }
  }
}

#endif