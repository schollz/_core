// Copyright 2026 Zack Scholl, GPLv3.0
#ifndef SHARP_MEMORY_H
#define SHARP_MEMORY_H

#include <stdint.h>
#include <stdbool.h>

#define SHARP_MEMORY_WIDTH 400
#define SHARP_MEMORY_HEIGHT 240
#define SHARP_MEMORY_ROW_BYTES (SHARP_MEMORY_WIDTH / 8)
#define SHARP_MEMORY_BUFFER_BYTES (SHARP_MEMORY_ROW_BYTES * SHARP_MEMORY_HEIGHT)

// Adafruit 4694 / Sharp LS027B7DH01, on the former Zeptocore knob pins.
// GPIO26 (blue): CLK; GPIO27 (purple): DI; GPIO28 (gray): active-high CS.
void sharp_memory_init(void);
void sharp_memory_clear(void);

// Row-major pixels: bit 0 is the leftmost pixel of each byte; 1 = white.
// Call every 500 ms to alternate VCOM, including when the image is unchanged.
void sharp_memory_refresh(const uint8_t *framebuffer);

// Foreground only. One SPI TX DMA channel, no interrupt handler or PIO.
// Calls poll even for an unchanged image, to maintain the 500 ms VCOM clock.
bool sharp_memory_async_init(void);
void sharp_memory_poll(void);
bool sharp_memory_busy(void);
// Up to eight rows, each 50 bytes, with the same bit layout as refresh().
bool sharp_memory_submit_rows(unsigned first, unsigned count, const uint8_t *rows);

#endif
