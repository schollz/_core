// Copyright 2026 Zack Scholl, GPLv3.0
// Standalone screen bring-up: no audio, SD-card access, ADC, or PIO required.
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "sharp_memory.h"
#include "font.h"

static uint8_t framebuffer[SHARP_MEMORY_BUFFER_BYTES];

// Retained as a simple SWD-readable heartbeat as well as USB serial output.
volatile uint32_t screen_test_frames;
volatile uint32_t screen_test_seconds;

static void pixel(int x, int y, bool black) {
  if (x < 0 || y < 0 || x >= SHARP_MEMORY_WIDTH || y >= SHARP_MEMORY_HEIGHT) {
    return;
  }
  uint8_t *byte = &framebuffer[y * SHARP_MEMORY_ROW_BYTES + x / 8];
  uint8_t mask = (uint8_t)(1u << (x % 8));
  if (black) {
    *byte &= (uint8_t)~mask;
  } else {
    *byte |= mask;
  }
}

static void rectangle(int x, int y, int width, int height, bool black) {
  for (int dy = 0; dy < height; ++dy) {
    for (int dx = 0; dx < width; ++dx) {
      pixel(x + dx, y + dy, black);
    }
  }
}

static void text(int x, int y, const char *string, int scale) {
  const unsigned height = font_8x5[0];
  const unsigned width = font_8x5[1];
  for (; *string; ++string, x += (int)(width + font_8x5[2]) * scale) {
    unsigned ch = (unsigned char)*string;
    if (ch < font_8x5[3] || ch > font_8x5[4]) {
      ch = '?';
    }
    const uint8_t *glyph = &font_8x5[5 + (ch - font_8x5[3]) * width];
    for (unsigned dx = 0; dx < width; ++dx) {
      for (unsigned dy = 0; dy < height; ++dy) {
        if (glyph[dx] & (1u << dy)) {
          rectangle(x + (int)dx * scale, y + (int)dy * scale, scale, scale, true);
        }
      }
    }
  }
}

static void centered_text(int y, const char *string, int scale) {
  int width = (int)strlen(string) * (font_8x5[1] + font_8x5[2]) * scale;
  text((SHARP_MEMORY_WIDTH - width) / 2, y, string, scale);
}

static void draw_test(uint32_t seconds, bool blink) {
  memset(framebuffer, 0xff, sizeof(framebuffer));
  rectangle(4, 4, 392, 2, true);
  rectangle(4, 234, 392, 2, true);
  rectangle(4, 4, 2, 232, true);
  rectangle(394, 4, 2, 232, true);
  centered_text(20, "ZEPTOCORE", 5);
  centered_text(68, "SCREEN TEST", 3);
  rectangle(20, 99, 360, 2, true);
  centered_text(110, "HELLO WORLD!", 3);
  centered_text(146, "26 BLUE CLK  27 PURPLE DI  28 GRAY CS", 1);
  for (int row = 0; row < 2; ++row) {
    for (int col = 0; col < 30; ++col) {
      if ((row + col) % 2 == 0) {
        rectangle(20 + col * 12, 168 + row * 12, 12, 12, true);
      }
    }
  }
  char counter[32];
  snprintf(counter, sizeof(counter), "SECONDS %06lu", (unsigned long)seconds);
  centered_text(208, counter, 2);
  rectangle(352, 206, 20, 20, blink);
}

int main(void) {
  stdio_init_all();
  gpio_init(25);  // WeAct onboard user LED, independent of display wiring.
  gpio_set_dir(25, GPIO_OUT);
  sharp_memory_init();
  sharp_memory_clear();

  absolute_time_t next_frame = get_absolute_time();
  while (true) {
    screen_test_seconds = to_ms_since_boot(get_absolute_time()) / 1000;
    bool blink = (screen_test_frames & 1u) != 0;
    draw_test(screen_test_seconds, blink);
    sharp_memory_refresh(framebuffer);
    gpio_put(25, blink);
    ++screen_test_frames;
    printf("zeptocore-screen: seconds=%lu frames=%lu CLK=26 DI=27 CS=28\n",
           (unsigned long)screen_test_seconds, (unsigned long)screen_test_frames);
    next_frame = delayed_by_ms(next_frame, 500);
    sleep_until(next_frame);
  }
}
