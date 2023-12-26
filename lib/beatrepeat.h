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
#define BEATREPEAT_RINGBUFFER_SIZE 22050
#define BEATREPEAT_ZEROCROSSING_SIZE 1000
#include "fixedpoint.h"
//
#include "crossfade3.h"
#include "stdbool.h"

typedef struct BeatRepeat {
  // buffer for holding original samples
  int16_t ringbuffer[BEATREPEAT_RINGBUFFER_SIZE];
  int16_t ringbuffer_index;
  // repeat state
  int16_t repeat_start;
  int16_t repeat_end;
  int16_t repeat_index;
  // crossfading repeats
  int16_t crossfade_in;
  int16_t crossfade_out;
  // zerocrossings for selecting best repeat
  int16_t zerocrossings[BEATREPEAT_ZEROCROSSING_SIZE];
  int16_t zerocrossings_index;
  int16_t last;
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

  self->zerocrossings_index = 0;
  for (int i = 0; i < BEATREPEAT_ZEROCROSSING_SIZE; i++) {
    self->zerocrossings[i] = -1;
  }
  return self;
}

void BeatRepeat_process(BeatRepeat *self, int16_t *samples,
                        int16_t num_samples) {
  for (int ii = 0; ii < num_samples; ii++) {
    int16_t sample = samples[ii];
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
            q16_16_multiply(Q16_16_1 - crossfade3_line[self->crossfade_out],
                            q16_16_int16_to_fp(sample)) +
            q16_16_multiply(crossfade3_line[self->crossfade_out],
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
      samples[ii] = sample;
    } else {
      // do zerocrossing detection
      if (self->last < 0 && sample >= 0) {
        self->zerocrossings[self->zerocrossings_index] = self->ringbuffer_index;
        self->zerocrossings_index++;
        if (self->zerocrossings_index > BEATREPEAT_ZEROCROSSING_SIZE) {
          self->zerocrossings_index = 0;
        }
      }
      self->ringbuffer[self->ringbuffer_index] = sample;
      self->ringbuffer_index++;
      if (self->ringbuffer_index > BEATREPEAT_RINGBUFFER_SIZE) {
        self->ringbuffer_index = 0;
      }
      self->last = sample;
    }
  }
}

void BeatRepeat_repeat(BeatRepeat *self, int16_t num_samples) {
  if (num_samples == 0) {
    if (self->repeat_start > -1 && self->repeat_end > -1) {
      self->crossfade_in = CROSSFADE3_LIMIT;
      self->crossfade_out = 0;
    }
    return;
  }
  if (self->zerocrossings_index < 2) {
    return;
  }
  self->crossfade_in = 0;
  self->crossfade_out = CROSSFADE3_LIMIT;

  self->repeat_end = self->zerocrossings[self->zerocrossings_index - 1];
  // find the first repeat_start that exceeds the requested num_samples
  for (int j = 0; j < BEATREPEAT_ZEROCROSSING_SIZE; j++) {
    int i = self->zerocrossings_index - j - 1;
    if (i < 0) {
      i += BEATREPEAT_ZEROCROSSING_SIZE;
    }
    if (self->zerocrossings[i] > -1) {
      self->repeat_start = self->zerocrossings[i];
      int16_t diff = self->repeat_end - self->repeat_start;
      if (diff < 0) {
        diff += BEATREPEAT_RINGBUFFER_SIZE;
      }
      if (diff > num_samples) {
        break;
      }
    }
  }
  self->repeat_index = self->repeat_start;
  fprintf(stderr, "repeating %d->%d\n", self->repeat_start, self->repeat_end);
}

void BeatRepeat_free(BeatRepeat *self) { free(self); }

#endif /* BEATREPEAT_LIB */
