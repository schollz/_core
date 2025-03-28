// Copyright 2023-2025 Zack Scholl, GPLv3.0

#ifndef LIB_LEDText
#define LIB_LEDText

#include "leds.h"
#include "tinyfont.h"

#define LEDTEXT_DEBOUNCE_TIME 200;

typedef struct LEDText {
  uint16_t debounce;
  char text[10];
  uint8_t i;
} LEDText;

LEDText *LEDText_create() {
  LEDText *lt = (LEDText *)malloc(sizeof(LEDText));
  lt->debounce = 0;
  lt->i = 0;
  return lt;
}

void LEDText_showGlyph(LEDText *lt, LEDS *leds, uint8_t char_glyph) {
  uint8_t char_index = (char_glyph - 32) / 2 * 4;
  uint8_t char_side = 1 - (char_glyph % 2);

  for (uint8_t i = 0; i < 4; i++) {
    uint8_t b = tinyfont_glyphs[char_index + i];
    printf("0x%02X: ", b);
    for (uint8_t j = 4 - (char_side * 4); j < 8 - char_side * 4; j++) {
      uint8_t led_index = (j * 4 + i + 4 - (16 * (1 - char_side)));
      // print out the jth bit of the byte b
      printf("%d (%2d) ", (b >> j) & 1, led_index);
      // read the jth bit of the byte b
      LEDS_set(leds, led_index, 2 * ((b >> j) & 1));
    }
    printf("\n");
  }
  LEDS_render(leds);
  lt->debounce = LEDTEXT_DEBOUNCE_TIME;
}

void LEDText_display(LEDText *lt, char *text) {
  for (int i = 0; i < strlen(text); i++) {
    lt->text[i] = text[i];
  }
  lt->text[strlen(text)] = '\0';
  lt->debounce = 1;
}

void LEDText_displayNumber(LEDText *lt, uint16_t number) {
  char number_string[8];
  sprintf(number_string, "%d", number);
  LEDText_display(lt, number_string);
}

void LEDText_update(LEDText *lt, LEDS *leds) {
  if (lt->debounce > 0) {
    lt->debounce--;
    if (lt->debounce == 0) {
      if (lt->i < strlen(lt->text)) {
        LEDText_showGlyph(lt, leds, lt->text[lt->i]);
        lt->i++;
      } else {
        LEDS_clear(leds);
      }
    }
  }
}

#endif
