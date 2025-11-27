// Copyright 2023-2025 Zack Scholl, GPLv3.0
#ifndef KNOB_CHANGE_LIB
#define KNOB_CHANGE_LIB 1

#include <math.h>  // needed for abs()
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>  // needed for malloc, free

#define FILTER_WINDOW_SIZE 9  // Number of samples for smoothing
#define CHANGE_THRESHOLD 6    // Minimum difference to register a change

typedef struct KnobChange {
  int values[FILTER_WINDOW_SIZE];
  uint8_t index;
  uint8_t count;
  int32_t sum;  // prevent overflow
  int last_output;
} KnobChange;

KnobChange *KnobChange_malloc(int16_t threshold) {
  KnobChange *filter = (KnobChange *)malloc(sizeof(KnobChange));
  filter->index = 0;
  filter->sum = 0;
  filter->count = 0;
  filter->last_output = INT32_MIN;  // clearer "uninitialized" flag
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
  filter->index = (filter->index + 1) % FILTER_WINDOW_SIZE;

  // Compute smoothed value (better smoothing once buffer is full)
  int smoothed_value;
  if (filter->count == FILTER_WINDOW_SIZE) {
    smoothed_value = filter->sum / FILTER_WINDOW_SIZE;
  } else {
    smoothed_value = filter->sum / filter->count;  // startup behavior
  }

  // Check if change is significant
  if (filter->last_output == INT32_MIN ||
      abs(smoothed_value - filter->last_output) >= CHANGE_THRESHOLD) {
    filter->last_output = smoothed_value;
    return smoothed_value;
  }

  return -1;  // No significant change
}

// Reset filter to initial state
void KnobChange_reset(KnobChange *filter) {
  filter->index = 0;
  filter->count = 0;
  filter->sum = 0;
  filter->last_output = INT32_MIN;
  for (int i = 0; i < FILTER_WINDOW_SIZE; i++) {
    filter->values[i] = 0;
  }
}

#endif
