// Copyright 2023-2025 Zack Scholl, GPLv3.0

#ifndef COMB_LIB
#define COMB_LIB 1
#define COMB_RINGBUFFER_SIZE 2000  // samples
#include "fixedpoint.h"
//
#if SAMPLES_PER_BUFFER == 64
#include "crossfade4_64.h"
#endif
#if SAMPLES_PER_BUFFER == 128
#include "crossfade4_128.h"
#endif
#if SAMPLES_PER_BUFFER == 160
#include "crossfade4_160.h"
#endif
#if SAMPLES_PER_BUFFER == 192
#include "crossfade4_192.h"
#endif
#if SAMPLES_PER_BUFFER == 256
#include "crossfade4_256.h"
#endif
#if SAMPLES_PER_BUFFER == 441
#include "crossfade4_441.h"
#endif
#include "stdbool.h"

typedef struct Comb {
  // buffer for holding original samples
  int32_t ringbuffer[2][COMB_RINGBUFFER_SIZE];
  int16_t index;
  int32_t feedback[2];  // fixed point
  int16_t duration[2];
  uint8_t spread[2];
  bool on;
} Comb;

Comb *Comb_malloc() {
  Comb *self = (Comb *)malloc(sizeof(Comb));
  self->index = 0;
  self->feedback[0] = q16_16_float_to_fp(0.96);
  self->feedback[1] = q16_16_float_to_fp(0.96);
  self->duration[0] = 1323;
  self->duration[1] = 1323;
  self->spread[0] = 10;
  self->spread[1] = 10;
  self->on = false;
  for (int i = 0; i < COMB_RINGBUFFER_SIZE; i++) {
    self->ringbuffer[0][i] = 0;
    self->ringbuffer[1][i] = 0;
  }
  return self;
}

void __not_in_flash_func(Comb_process)(Comb *self, int32_t *samples, uint16_t num_samples) {
  for (int ii = 0; ii < num_samples; ii++) {
    if (!self->on) {
      self->ringbuffer[0][self->index] = samples[ii * 2 + 0];
      self->ringbuffer[1][self->index] = samples[ii * 2 + 1];
    } else {
      for (uint8_t ch = 0; ch < 2; ch++) {
        int16_t index_delay = self->index - self->duration[ch];
        if (index_delay < 0) {
          index_delay += COMB_RINGBUFFER_SIZE;
        }
        int64_t val =
            (int64_t)samples[ii * 2 + ch] +
            (int64_t)q16_16_multiply(self->feedback[ch],
                                     self->ringbuffer[ch][index_delay]);
        if (val > 32000) {
          // wave fold
          val = 65535 - val;
        } else if (val < -32000) {
          val = -65536 - val;
        }
        self->ringbuffer[ch][self->index] = val;
        samples[ii * 2 + ch] = (int64_t)self->ringbuffer[ch][self->index] / 4;
      }
    }
    self->index++;
    if (self->index >= COMB_RINGBUFFER_SIZE) {
      self->index = 0;
    }
  }
}

#define FEEDBACK_MAXSPREAD 3267
#define FEEDBACK_MID ((Q16_16_1 - 1000) - (2 * 3267))
#define DURATION_MAXSPREAD ((COMB_RINGBUFFER_SIZE - 100) / 2)
#define DURATION_MID (COMB_RINGBUFFER_SIZE / 2)

void Comb_setActive(Comb *self, bool on, uint8_t duration_spread,
                    uint8_t feedback_spread) {
  if (!self->on) {
    for (uint8_t ch = 0; ch < 2; ch++) {
      self->duration[ch] = random_integer_in_range(
          DURATION_MID - (duration_spread * DURATION_MAXSPREAD / 255),
          DURATION_MID + (duration_spread * DURATION_MAXSPREAD / 255));
      self->feedback[ch] = random_integer_in_range(
          FEEDBACK_MID - (feedback_spread * FEEDBACK_MAXSPREAD / 255),
          FEEDBACK_MID + (feedback_spread * FEEDBACK_MAXSPREAD / 255));
    }
  }
  self->on = on;
}

void Comb_free(Comb *self) { free(self); }

#endif /* COMB_LIB */
