#ifndef Delay_LIB
#define Delay_LIB 1

#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#include "fixedpoint.h"
#include "dsp_multiply.h"
#include "slew.h"

typedef struct Delay {
  int32_t buffer[10000];      // Fixed circular buffer
  size_t buffer_size;         // Size of the circular buffer
  size_t write_index;         // Current write index
  float delay_time;           // Delay time in samples (can be fractional)
  float smoothed_delay_time;  // Smoothed delay time for interpolation
  float feedback;
  int32_t feedback_fp;
  uint8_t wet;
  bool on;
} Delay;

void Delay_setFeedback(Delay *self, uint8_t feedback) {
  self->feedback = 0.5f + (float)feedback / 500.0f;
  if (self->feedback > 0.99f) {
    self->feedback = 0.99f;
  }
  self->feedback_fp = q16_16_float_to_fp(self->feedback);
}

void Delay_setFeedbackf(Delay *self, float feedback) {
  self->feedback = feedback;
  self->feedback_fp = q16_16_float_to_fp(self->feedback);
}

void Delay_setDuration(Delay *tapeDelay, float delay_time) {
  tapeDelay->delay_time = delay_time;
}

Delay *Delay_malloc() {
  Delay *tapeDelay = (Delay *)malloc(sizeof(Delay));
  if (tapeDelay == NULL) {
    return NULL;
  }

  tapeDelay->buffer_size = 10000;  // Fixed buffer size
  tapeDelay->delay_time = tapeDelay->buffer_size;
  tapeDelay->smoothed_delay_time = tapeDelay->delay_time;
  tapeDelay->write_index = 0;
  tapeDelay->on = false;

  // Initialize the buffer to zero
  for (size_t i = 0; i < tapeDelay->buffer_size; i++) {
    tapeDelay->buffer[i] = 0;
  }

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
const int64_t range = (int64_t)INT32_MAX * 7 / 8;
static inline int32_t softclip(int64_t x) {
  if (x > range) {
    return (int32_t)range;
  } else if (x < -range) {
    return (int32_t)(-range);
  } else {
    return x;
    // return (int32_t)(x / (1 + abs(x) / range));  // Smoother compression
  }
}

// Function to add two int64_t values with soft clipping
static inline int32_t add_and_softclip(int64_t a, int64_t b) {
  return softclip(a + b);
}

void Delay_process(Delay *tapeDelay, int32_t *samples, unsigned int nr_samples,
                   uint8_t channel) {
  if (tapeDelay == NULL || !tapeDelay->on) {
    return;
  }

  const size_t buffer_size = tapeDelay->buffer_size;
  const float buffer_size_f = (float)buffer_size;
  size_t write_index = tapeDelay->write_index;
  float smoothed_delay_time = tapeDelay->smoothed_delay_time;
  for (unsigned int i = 0; i < nr_samples; i++) {
    // Smooth the delay time for gradual transitions
    smoothed_delay_time +=
        0.01f * (tapeDelay->delay_time - smoothed_delay_time);

    float fractional_read_index =
        (float)write_index - smoothed_delay_time;
    if (fractional_read_index < 0) {
      fractional_read_index += buffer_size_f;
    }

    const size_t integral_read_index = (size_t)fractional_read_index;
    // The normal delay range already gives an in-range index. Retain the
    // modulo fallback for other representable indices, including round-up at
    // the end of the ring, without dividing on every sample.
    size_t base_read_index = integral_read_index;
    if (base_read_index >= buffer_size) base_read_index %= buffer_size;
    size_t next_read_index = base_read_index + 1;
    if (next_read_index == buffer_size) next_read_index = 0;
    float frac = fractional_read_index - integral_read_index;

    // Read the delayed sample with interpolation
    int32_t delayed_sample =
        linear_interpolation(tapeDelay->buffer[base_read_index],
                             tapeDelay->buffer[next_read_index], frac);

    // Add feedback to the current sample and write it to the buffer
    int32_t input_sample = samples[i * 2 + channel];
    int32_t feedback_sample =
        dsp_multiply_q16(tapeDelay->feedback_fp, delayed_sample);
    int32_t processed_sample = add_and_softclip(input_sample, feedback_sample);

    tapeDelay->buffer[write_index] = processed_sample;

    // Update write index
    if (++write_index == buffer_size) write_index = 0;

    // Store the processed sample back in the buffer
    samples[i * 2 + channel] = processed_sample;
    samples[i * 2 + 1] = add_and_softclip(samples[i * 2 + 1], feedback_sample);
  }
  tapeDelay->write_index = write_index;
  tapeDelay->smoothed_delay_time = smoothed_delay_time;
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
