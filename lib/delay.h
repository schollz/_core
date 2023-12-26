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

#ifndef DELAY_LIB
#define DELAY_LIB 1
#define DELAY_RINGBUFFER_SIZE 44100
#define DELAY_ZEROCROSSING_SIZE 10000
#include "fixedpoint.h"
//
#include "crossfade3.h"
#include "stdbool.h"
int32_t delay_ringbuffer[DELAY_RINGBUFFER_SIZE];

typedef struct Delay {
  // buffer for holding original samples
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
  self->duration = 22050;
  self->on = false;
  self->crossfade_in = CROSSFADE3_LIMIT;
  self->crossfade_out = CROSSFADE3_LIMIT;
  for (int i = 0; i < DELAY_RINGBUFFER_SIZE; i++) {
    delay_ringbuffer[i] = 0;
  }
  return self;
}

void Delay_setDuration(Delay *self, uint16_t num_samples) {
  self->duration = num_samples;
}

void Delay_process(Delay *self, int32_t *samples, uint16_t num_samples) {
  for (int ii = 0; ii < num_samples; ii++) {
    while (self->ringbuffer_index > self->duration) {
      self->ringbuffer_index -= self->duration;
    }
    delay_ringbuffer[self->ringbuffer_index] =
        delay_ringbuffer[self->ringbuffer_index] >> self->feedback;
    self->ringbuffer_index++;
    if (self->ringbuffer_index == self->duration) {
      self->ringbuffer_index = 0;
    }

    if (self->crossfade_in < CROSSFADE3_LIMIT) {
      samples[ii] += q16_16_fp_to_int16(q16_16_multiply(
          Q16_16_1 - crossfade3_line[self->crossfade_in],
          q16_16_int16_to_fp(delay_ringbuffer[self->ringbuffer_index])));
      self->crossfade_in++;
      if (self->crossfade_in == CROSSFADE3_LIMIT) {
        self->on = true;
      }
    } else if (self->crossfade_out < CROSSFADE3_LIMIT) {
      samples[ii] += q16_16_fp_to_int16(q16_16_multiply(
          crossfade3_line[self->crossfade_out],
          q16_16_int16_to_fp(delay_ringbuffer[self->ringbuffer_index])));
      self->crossfade_out++;
      if (self->crossfade_out >= CROSSFADE3_LIMIT) {
        self->on = false;
      }
    } else if (self->on) {
      samples[ii] += delay_ringbuffer[self->ringbuffer_index];
    }

    delay_ringbuffer[self->ringbuffer_index] = samples[ii];
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
