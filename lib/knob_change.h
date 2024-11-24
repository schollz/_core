// Copyright 2023-2024 Zack Scholl.
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

#ifndef KNOB_CHANGE_LIB
#define KNOB_CHANGE_LIB 1

typedef struct KnobChange {
  int16_t last;
  int16_t debounce : 12;
  int16_t threshold : 4;
} KnobChange;

KnobChange *KnobChange_malloc(int16_t threshold) {
  KnobChange *self = (KnobChange *)malloc(sizeof(KnobChange));
  if (threshold > 7) {
    threshold = 7;
  } else if (threshold < 0) {
    threshold = -1 * threshold;
  }
  self->last = -1;
  self->debounce = 0;
  self->threshold = threshold;
  return self;
}

void KnobChange_free(KnobChange *self) { free(self); }

int16_t KnobChange_update(KnobChange *self, int16_t val) {
  if (self->debounce > 0) {
    self->debounce--;
  }
  if (self->last == -1) {
    self->last = val;
    return val;
  }
  int16_t diff = val - self->last;
  if (diff > self->threshold || diff < -self->threshold) {
    self->last = val;
    self->debounce = 10;
    return val;
  } else if (self->debounce > 0) {
    return self->last;
  }
  return -1;
}

// reset debounce
void KnobChange_reset(KnobChange *self) { self->debounce = 0; }

#endif