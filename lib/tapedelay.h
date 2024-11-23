#ifndef Delay_LIB
#define Delay_LIB 1

#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#include "fixedpoint.h"
#include "slew.h"

typedef struct Delay {
  int32_t buffer[10000];  // Fixed circular buffer of 22000 samples
  size_t buffer_size;     // Size of the circular buffer
  size_t write_index;     // Current write index
  float delay_time;       // Delay time in samples (can be fractional)
  float feedback;
  int32_t feedback_fp;
  uint8_t wet;
  Slew feedback_slew;
  Slew delay_slew;
  bool on;
} Delay;

void Delay_setFeedback(Delay *self, uint8_t feedback) {
  self->feedback = 0.5f + (float)feedback / 500.0f;
  if (self->feedback > 0.99f) {
    self->feedback = 0.99f;
  }
  self->feedback_fp = q16_16_float_to_fp(self->feedback);

  // Slew_set_target(&self->feedback_slew, self->feedback,
  //                 random_integer_in_range(10, 200));
}

void Delay_setFeedbackf(Delay *self, float feedback) {
  self->feedback = feedback;
  self->feedback_fp = q16_16_float_to_fp(self->feedback);
  // Slew_set_target(&self->feedback_slew, self->feedback,
  //                 random_integer_in_range(10, 200));
}

void Delay_setDuration(Delay *tapeDelay, float delay_time) {
  tapeDelay->delay_time = delay_time;
  // Slew_set_target(&tapeDelay->delay_slew, delay_time,
  //                 random_integer_in_range(10, 300));
}

Delay *Delay_malloc() {
  Delay *tapeDelay = (Delay *)malloc(sizeof(Delay));
  if (tapeDelay == NULL) {
    return NULL;
  }

  tapeDelay->buffer_size = 10000;  // Fixed buffer size
  tapeDelay->delay_time = tapeDelay->buffer_size;
  tapeDelay->write_index = 0;
  tapeDelay->on = false;

  // Initialize the buffer to zero
  for (size_t i = 0; i < tapeDelay->buffer_size; i++) {
    tapeDelay->buffer[i] = 0;
  }

  Slew_init(&tapeDelay->feedback_slew, 94230, 0);
  Slew_init(&tapeDelay->delay_slew, 94230, 0);

  Delay_setDuration(tapeDelay, tapeDelay->buffer_size / 2);
  Delay_setFeedback(tapeDelay, 200);

  return tapeDelay;
}

void Delay_setWet(Delay *self, uint8_t wet) {
  // do nothing
}

void Delay_setLength(Delay *self, uint8_t length) {
  // do nothing
}

// Linear interpolation helper
static inline int32_t linear_interpolation(int32_t y1, int32_t y2, float frac) {
  return y1 + (int32_t)((y2 - y1) * frac);
}

// Soft clip function for int32_t range
static inline int32_t softclip(int64_t x) {
  int64_t range = (int64_t)INT32_MAX;

  // Clamp values out of bounds directly to INT32_MAX or INT32_MIN
  if (x > range) {
    return (int32_t)range;
  } else if (x < -range) {
    return (int32_t)(-range);
  } else {
    // More aggressive soft clipping: f(x) = x / (1 + |x| / range)
    int64_t abs_x = (x < 0) ? -x : x;           // Absolute value of x
    return (int32_t)(x / (1 + abs_x / range));  // Apply aggressive compression
  }
}

// Function to add two int64_t values with soft clipping
static inline int32_t add_and_softclip(int64_t a, int64_t b) {
  return softclip(a + b);
}

void Delay_process(Delay *tapeDelay, int32_t *samples, unsigned int nr_samples,
                   uint8_t channel) {
  if (tapeDelay == NULL) {
    return;
  }
  if (tapeDelay->on == false) {
    return;
  }
  int32_t feedback = tapeDelay->feedback_fp;
  float delay_time = tapeDelay->delay_time;
  float previous_delay_time = delay_time;
  //  float previous_delay_time =
  //     Slew_process(&tapeDelay->delay_slew);  // Get initial delay time
  // // Update feedback and delay time dynamically
  // int32_t feedback =
  //     q16_16_float_to_fp(Slew_process(&tapeDelay->feedback_slew));
  // float delay_time = Slew_process(&tapeDelay->delay_slew);

  for (unsigned int i = 0; i < nr_samples; i++) {
    // If delay time changes, introduce abrupt changes to fractional index
    float fractional_read_index = (float)tapeDelay->write_index - delay_time;
    if (fractional_read_index < 0) {
      fractional_read_index += tapeDelay->buffer_size;
    }

    // Adjust fractional read index aggressively for pitchy artifacts
    if (fabs(delay_time - previous_delay_time) >
        0.01f) {  // Threshold to detect significant change
      fractional_read_index +=
          (delay_time - previous_delay_time) * 0.5f;  // Emphasize pitch change
      previous_delay_time = delay_time;
    }

    size_t base_read_index =
        (size_t)fractional_read_index % tapeDelay->buffer_size;
    size_t next_read_index = (base_read_index + 1) % tapeDelay->buffer_size;
    float frac = fractional_read_index - (size_t)fractional_read_index;

    // Read the delayed sample with interpolation
    int32_t delayed_sample =
        linear_interpolation(tapeDelay->buffer[base_read_index],
                             tapeDelay->buffer[next_read_index], frac);

    // Add feedback to the current sample and write it to the buffer
    int32_t input_sample = samples[i * 2 + channel];
    int32_t feedback_sample = q16_16_multiply(feedback, delayed_sample);
    int32_t processed_sample = add_and_softclip(input_sample, feedback_sample);

    tapeDelay->buffer[tapeDelay->write_index] = processed_sample;

    // Update write index
    tapeDelay->write_index =
        (tapeDelay->write_index + 1) % tapeDelay->buffer_size;

    // Store the processed sample back in the buffer
    samples[i * 2 + channel] = processed_sample;
    samples[i * 2 + 1] = add_and_softclip(samples[i * 2 + 1], feedback_sample);
  }
}

void Delay_setActive(Delay *self, bool on) {
  if (on) {
    // reset the delay buffer
    for (size_t i = 0; i < self->buffer_size; i++) {
      self->buffer[i] = 0;
    }
  }
  self->on = on;
}

void Delay_free(Delay *tapeDelay) {
  if (tapeDelay != NULL) {
    free(tapeDelay);
  }
}

#endif
