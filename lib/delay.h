// Copyright 2023-2025 Zack Scholl, GPLv3.0

#ifndef DELAY_LIB
#define DELAY_LIB 1
#define DELAY_RINGBUFFER_SIZE (11025 - 1000)
#define DELAY_RINGBUFFER_SIZE_MINUS (10925 - 1000)
#define DELAY_ZEROCROSSING_SIZE (10000 - 1000)
#include "fixedpoint.h"
//
#include "crossfade4.h"
#include "stdbool.h"

typedef struct Delay {
  // buffer for holding original samples
  int32_t delay_ringbuffer[DELAY_RINGBUFFER_SIZE];
  uint16_t ringbuffer_index;
  uint8_t feedback;  // (1-16)
  uint16_t duration;
  bool on;

  // crossfading repeats
  int16_t crossfade_in;
  int16_t crossfade_out;
} Delay;

Delay *Delay_malloc() {
  Delay *self = (Delay *)malloc(sizeof(Delay));
  self->ringbuffer_index = 0;
  self->feedback = 1;
  self->duration = 11025;
  self->on = false;
  self->crossfade_in = CROSSFADE3_LIMIT;
  self->crossfade_out = CROSSFADE3_LIMIT;
  for (int i = 0; i < DELAY_RINGBUFFER_SIZE; i++) {
    self->delay_ringbuffer[i] = 0;
  }
  return self;
}

void Delay_setDuration(Delay *self, uint16_t num_samples) {
  if (num_samples > DELAY_RINGBUFFER_SIZE) {
    num_samples = DELAY_RINGBUFFER_SIZE;
  }
  if (num_samples != self->duration) {
    printf("[delay] duration %d\n", num_samples);
    self->duration = num_samples;
  }
}

void Delay_setLength(Delay *self, uint8_t length) {
  Delay_setDuration(self, 100 + ((DELAY_RINGBUFFER_SIZE_MINUS * length) >> 8));
}

void Delay_setFeedback(Delay *self, uint8_t feedback) {
  if (feedback > 16) {
    feedback = 16;
  }
  if (feedback != self->feedback) {
    printf("[delay] feedback %d\n", feedback);
    self->feedback = feedback;
  }
}

void Delay_setFeedbackf(Delay *self, float feedback) {
  Delay_setFeedback(self, (uint8_t)(feedback * 8.0f));
}

void __not_in_flash_func(Delay_process)(Delay *self, int32_t *samples, uint16_t num_samples,
                   uint8_t channel) {
  for (int ii = 0; ii < num_samples; ii++) {
    while (self->ringbuffer_index > self->duration) {
      self->ringbuffer_index -= self->duration;
    }
    self->delay_ringbuffer[self->ringbuffer_index] =
        self->delay_ringbuffer[self->ringbuffer_index] >> self->feedback;
    self->ringbuffer_index++;
    if (self->ringbuffer_index == self->duration) {
      self->ringbuffer_index = 0;
    }

    if (self->crossfade_in < CROSSFADE3_LIMIT) {
      int32_t v =
          q16_16_multiply(Q16_16_1 - crossfade3_line[self->crossfade_in],
                          self->delay_ringbuffer[self->ringbuffer_index]);
      samples[ii * 2 + 0] += v;
      samples[ii * 2 + 1] += v;
      self->crossfade_in++;
      if (self->crossfade_in == CROSSFADE3_LIMIT) {
        self->on = true;
      }
    } else if (self->crossfade_out < CROSSFADE3_LIMIT) {
      int32_t v =
          q16_16_multiply(crossfade3_line[self->crossfade_out],
                          self->delay_ringbuffer[self->ringbuffer_index]);
      samples[ii * 2 + 0] += v;
      samples[ii * 2 + 1] += v;
      self->crossfade_out++;
      if (self->crossfade_out >= CROSSFADE3_LIMIT) {
        self->on = false;
      }
    } else if (self->on) {
      samples[ii * 2 + 0] += self->delay_ringbuffer[self->ringbuffer_index];
      samples[ii * 2 + 1] += self->delay_ringbuffer[self->ringbuffer_index];
    }

    self->delay_ringbuffer[self->ringbuffer_index] = samples[ii * 2 + channel];
  }
}

void Delay_setActive(Delay *self, bool on) {
  if (on) {
    self->crossfade_in = 0;
    self->crossfade_out = CROSSFADE3_LIMIT;
  } else if (self->on) {
    self->crossfade_in = CROSSFADE3_LIMIT;
    self->crossfade_out = 0;
  }
}

void Delay_free(Delay *self) { free(self); }

#endif /* DELAY_LIB */
