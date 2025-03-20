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

#include <stdbool.h>
#include <stdint.h>

#define FILTER_WINDOW_SIZE 6  // Number of samples for smoothing
#define CHANGE_THRESHOLD 3    // Minimum difference to register a change

typedef struct KnobChange {
  int values[FILTER_WINDOW_SIZE];
  uint8_t index;
  uint8_t count;
  int sum;
  int last_output;
} KnobChange;

KnobChange *KnobChange_malloc(int16_t threshold) {
  KnobChange *filter = (KnobChange *)malloc(sizeof(KnobChange));
  filter->index = 0;
  filter->sum = 0;
  filter->count = 0;
  filter->last_output = -1;
  for (int i = 0; i < FILTER_WINDOW_SIZE; i++) {
    filter->values[i] = 0;
  }
  return filter;
}

void KnobChange_free(KnobChange *self) { free(self); }

int16_t KnobChange_update(KnobChange *filter, int16_t new_value) {
  // Remove oldest value from sum
  if (filter->count == FILTER_WINDOW_SIZE) {
    filter->sum -= filter->values[filter->index];
  } else {
    filter->count++;
  }

  // Add new value
  filter->values[filter->index] = new_value;
  filter->sum += new_value;

  // Move index forward
  filter->index++;
  if (filter->index == FILTER_WINDOW_SIZE) {
    filter->index = 0;
  }

  // Compute smoothed value
  int smoothed_value = filter->sum / filter->count;

  // Check if change is significant
  if (filter->last_output == -1 ||
      (smoothed_value - filter->last_output >= CHANGE_THRESHOLD) ||
      (filter->last_output - smoothed_value >= CHANGE_THRESHOLD)) {
    filter->last_output = smoothed_value;
    return smoothed_value;
  }

  return -1;
}

// Reset debounce
void KnobChange_reset(KnobChange *self) {}

#endif
