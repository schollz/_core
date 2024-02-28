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

#ifndef KNOB_CHANGE_LIB
#define KNOB_CHANGE_LIB 1
#define KNOB_CHANGE_THRESH 7
#define KNOB_CHANGE_DEBOUNCE 100

typedef struct KnobChange {
  int16_t last;
  uint8_t debounce;
} KnobChange;

KnobChange *KnobChange_malloc() {
  KnobChange *self = (KnobChange *)malloc(sizeof(KnobChange));
  self->last = -1;
  self->debounce = 0;
  return self;
}

void KnobChange_free(KnobChange *self) { free(self); }

int16_t KnobChange_update(KnobChange *self, int16_t val) {
  if (self->last == -1) {
    self->last = val;
    return -1;
  }
  if (self->debounce > 0) {
    self->debounce--;
  }
  int16_t diff = val - self->last;
  if (diff > KNOB_CHANGE_THRESH) {
    self->last = val;
    return val;
  } else if (diff < -KNOB_CHANGE_THRESH) {
    self->last = val;
    return val;
  } else if (self->debounce > 0) {
    return val;
  }
  return -1;
}

#endif