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

#ifndef BEATREPEAT_LIB
#define BEATREPEAT_LIB 1
#define BEATREPEAT_RINGBUFFER_SIZE 22100

#include "fixedpoint.h"
//
#include "crossfade3.h"
#include "stdbool.h"

typedef struct BeatRepeat {
  int16_t ringbuffer[BEATREPEAT_RINGBUFFER_SIZE];
  int16_t ringbuffer_index;
  int16_t repeat_start;
  int16_t repeat_end;
  int16_t repeat_index;
  int16_t crossfade_in;
  int16_t crossfade_out;
} BeatRepeat;

BeatRepeat *BeatRepeat_malloc() {
  BeatRepeat *self = (BeatRepeat *)malloc(sizeof(BeatRepeat));
  self->ringbuffer_index = 0;
  for (int i = 0; i < BEATREPEAT_RINGBUFFER_SIZE; i++) {
    self->ringbuffer[i] = 0;
  }
  self->repeat_start = -1;
  self->repeat_end = -1;
  self->repeat_index = 0;
  self->crossfade_in = CROSSFADE3_LIMIT;
  self->crossfade_out = CROSSFADE3_LIMIT;

  return self;
}

int16_t BeatRepeat_process(BeatRepeat *self, int16_t sample) {
  // do beat repeat
  if (self->repeat_start > -1 && self->repeat_end > -1) {
    int16_t sample2 = self->ringbuffer[self->repeat_index];
    if (self->crossfade_in < CROSSFADE3_LIMIT) {
      sample = q16_16_fp_to_int16(
          q16_16_multiply(crossfade3_line[self->crossfade_in],
                          q16_16_int16_to_fp(sample)) +
          q16_16_multiply(Q16_16_1 - crossfade3_line[self->crossfade_in],
                          q16_16_int16_to_fp(sample2)));
      self->crossfade_in++;
    } else if (self->crossfade_out < CROSSFADE3_LIMIT) {
      sample = q16_16_fp_to_int16(
          q16_16_multiply(Q16_16_1 - crossfade3_line[self->crossfade_in],
                          q16_16_int16_to_fp(sample)) +
          q16_16_multiply(crossfade3_line[self->crossfade_in],
                          q16_16_int16_to_fp(sample2)));
      self->crossfade_out++;
      if (self->crossfade_out == CROSSFADE3_LIMIT) {
        self->repeat_start = -1;
        self->repeat_end = -1;
        self->crossfade_in = CROSSFADE3_LIMIT;
        self->crossfade_out = CROSSFADE3_LIMIT;
      }
    } else {
      sample = sample2;
    }
    self->repeat_index++;
    if (self->repeat_index == self->repeat_end) {
      self->repeat_index = self->repeat_start;
    } else if (self->repeat_index > BEATREPEAT_RINGBUFFER_SIZE) {
      self->repeat_index = 0;
    }
    return sample;
  }

  self->ringbuffer[self->ringbuffer_index] = sample;
  self->ringbuffer_index++;
  if (self->ringbuffer_index > BEATREPEAT_RINGBUFFER_SIZE) {
    self->ringbuffer_index = 0;
  }
  return sample;
}

void BeatRepeat_repeat(BeatRepeat *self, int16_t num_samples) {
  if (num_samples == 0) {
    if (self->repeat_start > -1 && self->repeat_end > -1) {
      self->crossfade_in = CROSSFADE3_LIMIT;
      self->crossfade_out = 0;
    }
    return;
  }
  self->crossfade_in = 0;
  self->repeat_end = self->ringbuffer_index;
  self->repeat_start = self->ringbuffer_index - num_samples;
  if (self->repeat_start < 0) {
    self->repeat_start += BEATREPEAT_RINGBUFFER_SIZE;
  }
  self->repeat_index = self->repeat_start;
  fprintf(stderr, "repeating %d->%d\n", self->repeat_start, self->repeat_end);
}

void BeatRepeat_free(BeatRepeat *self) { free(self); }

#endif /* BEATREPEAT_LIB */
