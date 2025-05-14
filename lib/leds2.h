// Copyright 2023-2025 Zack Scholl, GPLv3.0

#ifndef LIB_LEDS
#define LIB_LEDS 1

#include "definitions.h"
#include "pca9552.h"

#define LEDS_ROWS 5
#define LEDS_COLS 4

int blink_time = 700;

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
  uint8_t gpio_leds_set;
  uint8_t gpio_leds_set_last;
  uint16_t rgb_leds_counter[LED_COUNT];
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
  leds->gpio_leds_set = 0;

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

#ifndef INCLUDE_ZEPTOMECH
  if (!is_arcade_box) {
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
  } else {
    blink_time = 70;
    uint8_t row_map[] = {// row 1
                         0, 0, 0, 0,
                         // row 2
                         1, 1, 1, 1,
                         // row 3
                         2, 2, 2, 2,
                         // row 4
                         3, 3, 3, 3};
    uint8_t col_map[] = {
        3, 2, 1, 0, 3, 2, 1, 0, 3, 2, 1, 0, 3, 2, 1, 0,
    };
    leds->pca = PCA9552_create(0x61, i2c_default, row_map, col_map);
  }

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
#endif

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
#ifdef INCLUDE_ZEPTOMECH
extern void LEDS_render_forward_zeptomech(LEDS* leds);
#endif

void LEDS_render(LEDS *leds) {

#ifndef INCLUDE_ZEPTOMECH
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
        if (is_arcade_box)
          leds->gpio_leds_set = bit_set(leds->gpio_leds_set, j, 0);
      } else if (leds->gpio_leds_state[j] == LED_BRIGHT) {
        gpio_put(leds->gpio_leds_pin[j], 1);
        if (is_arcade_box)
          leds->gpio_leds_set = bit_set(leds->gpio_leds_set, j, 1);
      }
    }
    // blink GPIO leds
    if (leds->gpio_leds_state[j] == LED_BLINK) {
      leds->gpio_leds_count[j]++;
      if (leds->gpio_leds_count[j] == blink_time) {
        gpio_put(leds->gpio_leds_pin[j], 1);
        if (is_arcade_box)
          leds->gpio_leds_set = bit_set(leds->gpio_leds_set, j, 1);
      } else if (leds->gpio_leds_count[j] >= blink_time * 2) {
        gpio_put(leds->gpio_leds_pin[j], 0);
        if (is_arcade_box)
          leds->gpio_leds_set = bit_set(leds->gpio_leds_set, j, 0);
        leds->gpio_leds_count[j] = 0;
      }
    }
  }
#else
  LEDS_render_forward_zeptomech(leds);
#endif

  if (is_arcade_box) {
    if (leds->gpio_leds_set != leds->gpio_leds_set_last) {
      leds->gpio_leds_set_last = leds->gpio_leds_set;
      mcp23017_set_pins_gpiob(i2c_default, MCP23017_ADDR2, leds->gpio_leds_set);
    }
  }
}

#endif