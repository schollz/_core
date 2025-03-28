// Copyright 2023-2025 Zack Scholl, GPLv3.0

#ifndef LIB_LEDS
#define LIB_LEDS 1

#include "pca9552.h"

#define LEDS_FACES 6
#define LEDS_ROWS 5
#define LEDS_COLS 4
#ifndef LED_1_GPIO
#define LED_1_GPIO 0
#endif
#ifndef LED_2_GPIO
#define LED_2_GPIO 19
#endif
#ifndef LED_3_GPIO
#define LED_3_GPIO 22
#endif
#ifndef LED_3_GPIO
#define LED_3_GPIO 29
#endif

#define LED_BASE_FACE 0
#define LED_STEP_FACE 1
#define LED_PRESS_FACE 2
#define LED_STEAL_FACE 3
#define LED_TEXT_FACE 4

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
  uint8_t state[LEDS_FACES][LEDS_ROWS][LEDS_COLS];
  PCA9552 *pca;
  uint8_t gpio_leds_state[4];
  uint8_t gpio_leds_count[4];
  uint8_t gpio_leds_pin[4];
} LEDS;

LEDS *LEDS_create() {
  LEDS *leds = (LEDS *)malloc(sizeof(LEDS));
  for (uint8_t m = 0; m < LEDS_FACES; m++) {
    for (uint8_t i = 0; i < LEDS_ROWS; i++) {
      for (uint8_t j = 0; j < LEDS_COLS; j++) {
        leds->state[m][i][j] = 0;
      }
    }
  }
  for (uint8_t i = 0; i < 4; i++) {
    leds->gpio_leds_count[i] = 0;
    leds->gpio_leds_state[i] = 0;
  }
  leds->gpio_leds_pin[0] = LED_1_GPIO;
  leds->gpio_leds_pin[1] = LED_2_GPIO;
  leds->gpio_leds_pin[2] = LED_3_GPIO;
  leds->gpio_leds_pin[3] = LED_4_GPIO;

  // my unique PCA9552 wiring requires a unique mapping
  uint8_t row_map[] = {// row 1
                       0, 0, 2, 3,
                       // row 2
                       1, 0, 3, 3,
                       // row 3
                       0, 1, 3, 2,
                       // row 4
                       1, 1, 2, 2};
  uint8_t col_map[] = {//
                       3, 2, 3, 0,
                       //
                       3, 0, 3, 2,
                       //
                       1, 0, 1, 0,
                       //
                       1, 2, 1, 2};

#ifdef INCLUDE_PCA9552
  leds->pca = PCA9552_create(0x60, i2c_default, row_map, col_map);
  if (leds->pca->error != PCA9552_OK) {
    printf("PCA9552_ERROR: %02x\n", leds->pca->error);
  }
#endif
  sleep_ms(1);
#ifdef INCLUDE_PCA9552
  PCA9552_clear(leds->pca);
  PCA9552_render(leds->pca);
#endif
  // setup GPIO leds (1,2,3)
  for (uint8_t i = 0; i < 3; i++) {
    gpio_init(leds->gpio_leds_pin[i]);
    gpio_set_dir(leds->gpio_leds_pin[i], GPIO_OUT);
    gpio_put(leds->gpio_leds_pin[i], 0);
  }

  return leds;
}

void LEDS_set(LEDS *leds, uint8_t face, uint8_t led, uint8_t state) {
  // printf("leds->state[%d][%d][%d]=%d\n", face, led / 4, led % 4, state);
  leds->state[face][led / 4][led % 4] = state;
}

void LEDS_clearAll(LEDS *leds, uint8_t m) {
  for (uint8_t i = 0; i < LEDS_ROWS; i++) {
    for (uint8_t j = 0; j < LEDS_COLS; j++) {
      leds->state[m][i][j] = 0;
    }
  }
}

void LEDS_render(LEDS *leds) {
  bool do_steal = false;
  bool do_text = false;
  for (uint8_t i = 0; i < LEDS_ROWS; i++) {
    for (uint8_t j = 0; j < LEDS_COLS; j++) {
      leds->state[LEDS_FACES - 1][i][j] = 0;
      for (uint8_t m = 0; m < LEDS_FACES - 1; m++) {
        if (leds->state[m][i][j] > 0) {
          leds->state[LEDS_FACES - 1][i][j] = leds->state[m][i][j];
          if (m == LED_STEAL_FACE) {
            do_steal = true;
          }
          if (m == LED_TEXT_FACE) {
            do_text = true;
          }
        }
      }
      if (do_text) {
        break;
      }
    }
    if (do_text) {
      break;
    }
  }

  if (do_text) {
    for (uint8_t i = 0; i < LEDS_ROWS; i++) {
      for (uint8_t j = 0; j < LEDS_COLS; j++) {
        leds->state[LEDS_FACES - 1][i][j] = leds->state[LED_TEXT_FACE][i][j];
      }
    }
  } else if (do_steal) {
    for (uint8_t i = 0; i < LEDS_ROWS; i++) {
      for (uint8_t j = 0; j < LEDS_COLS; j++) {
        leds->state[LEDS_FACES - 1][i][j] = leds->state[LED_STEAL_FACE][i][j];
      }
    }
  }

#ifdef INCLUDE_PCA9552
  // light up the PCA9552
  for (uint8_t i = 1; i < LEDS_ROWS; i++) {
    for (uint8_t j = 0; j < LEDS_COLS; j++) {
      PCA9552_ledSet(leds->pca, (i - 1) * 4 + j,
                     leds->state[LEDS_FACES - 1][i][j]);
    }
  }
  PCA9552_render(leds->pca);
#endif

#ifndef LEDS_NO_GPIO
  // light up the GPIO leds
  for (uint8_t j = 0; j < 4; j++) {
    if (leds->gpio_leds_state[j] != leds->state[LEDS_FACES - 1][0][j]) {
      leds->gpio_leds_state[j] = leds->state[LEDS_FACES - 1][0][j];
      if (leds->gpio_leds_state[j] == 0) {
        gpio_put(leds->gpio_leds_pin[j], 0);
      } else if (leds->gpio_leds_state[j] == 2) {
        gpio_put(leds->gpio_leds_pin[j], 1);
      }
    }
    // blink GPIO leds
    if (leds->gpio_leds_state[j] == 3) {
      leds->gpio_leds_count[j]++;
      if (leds->gpio_leds_count[j] == 128) {
        gpio_put(leds->gpio_leds_pin[j], 1);
      } else if (leds->gpio_leds_count[j] == 0) {
        gpio_put(leds->gpio_leds_pin[j], 0);
      }
    }
  }
#endif
}

void LEDS_show_blinking_z(LEDS *leds, uint8_t face) {
  LEDS_set(leds, face, 4, 3);
  LEDS_set(leds, face, 5, 3);
  LEDS_set(leds, face, 6, 3);
  LEDS_set(leds, face, 7, 3);
  LEDS_set(leds, face, 8, 0);
  LEDS_set(leds, face, 9, 0);
  LEDS_set(leds, face, 10, 3);
  LEDS_set(leds, face, 11, 0);
  LEDS_set(leds, face, 12, 0);
  LEDS_set(leds, face, 13, 3);
  LEDS_set(leds, face, 14, 0);
  LEDS_set(leds, face, 15, 0);
  LEDS_set(leds, face, 16, 3);
  LEDS_set(leds, face, 17, 3);
  LEDS_set(leds, face, 18, 3);
  LEDS_set(leds, face, 19, 3);
  LEDS_render(leds);
}

void LEDS_show_blinking_x(LEDS *leds, uint8_t face) {
  LEDS_set(leds, face, 4, 3);
  LEDS_set(leds, face, 5, 0);
  LEDS_set(leds, face, 6, 0);
  LEDS_set(leds, face, 7, 3);
  LEDS_set(leds, face, 8, 0);
  LEDS_set(leds, face, 9, 3);
  LEDS_set(leds, face, 10, 3);
  LEDS_set(leds, face, 11, 0);
  LEDS_set(leds, face, 12, 0);
  LEDS_set(leds, face, 13, 3);
  LEDS_set(leds, face, 14, 3);
  LEDS_set(leds, face, 15, 0);
  LEDS_set(leds, face, 16, 3);
  LEDS_set(leds, face, 17, 0);
  LEDS_set(leds, face, 18, 0);
  LEDS_set(leds, face, 19, 3);
  LEDS_render(leds);
}

#endif