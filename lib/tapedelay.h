#ifndef TapeDelay_LIB
#define TapeDelay_LIB 1

#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#include "fixedpoint.h"
#include "slew.h"

typedef struct TapeDelay {
  int32_t buffer[22000];  // Fixed circular buffer of 22000 samples
  size_t buffer_size;     // Size of the circular buffer
  size_t write_index;     // Current write index
  float delay_time;       // Delay time in samples (can be fractional)
  float feedback;
  Slew feedback_slew;
  Slew delay_slew;
} TapeDelay;

TapeDelay *TapeDelay_malloc(float feedback, float delay_time) {
  TapeDelay *tapeDelay = (TapeDelay *)malloc(sizeof(TapeDelay));
  if (tapeDelay == NULL) {
    return NULL;
  }

  tapeDelay->delay_time = delay_time;
  tapeDelay->buffer_size = 22000;  // Fixed buffer size
  tapeDelay->write_index = 0;
  tapeDelay->feedback = feedback;

  // Initialize the buffer to zero
  for (size_t i = 0; i < tapeDelay->buffer_size; i++) {
    tapeDelay->buffer[i] = 0;
  }

  Slew_init(&tapeDelay->feedback_slew, 94230, 0);
  Slew_set_target(&tapeDelay->feedback_slew, feedback, 94230);
  Slew_init(&tapeDelay->delay_slew, 94230, 0);
  Slew_set_target(&tapeDelay->delay_slew, delay_time, 94230);

  return tapeDelay;
}

void TapeDelay_set_feedback(TapeDelay *tapeDelay, float feedback) {
  tapeDelay->feedback = feedback;
  Slew_set_target(&tapeDelay->feedback_slew, feedback, 94230);
}

void TapeDelay_set_delay_time(TapeDelay *tapeDelay, float delay_time) {
  tapeDelay->delay_time = delay_time;
  Slew_set_target(&tapeDelay->delay_slew, delay_time, 94230);
}

// Linear interpolation helper
static inline int32_t linear_interpolation(int32_t y1, int32_t y2, float frac) {
  return y1 + (int32_t)((y2 - y1) * frac);
}

// Soft clipping function
static inline int32_t soft_clip(int32_t x, int32_t threshold) {
  if (x > threshold) {
    return threshold +
           ((x - threshold) >> 3);  // Gradual saturation above threshold
  } else if (x < -threshold) {
    return -threshold +
           ((x + threshold) >> 3);  // Gradual saturation below threshold
  } else {
    return x;  // No saturation within the threshold
  }
}

// Tanh-like saturation function
static inline int32_t tanh_saturation(int32_t x) {
  float normalized = x / (float)INT32_MAX;  // Normalize to range -1 to 1
  float saturated = tanh(normalized);       // Apply tanh for smooth compression
  return (int32_t)(saturated * INT32_MAX);  // Scale back to int32 range
}

// Fast tanh-like approximation
static inline int32_t tanh_approx(int32_t x) {
  float normalized = x / (float)INT32_MAX;  // Normalize to range -1 to 1
  float saturated =
      normalized * (27.0f + normalized * normalized) /
      (27.0f + 9.0f * normalized * normalized);  // Polynomial approximation
  return (int32_t)(saturated * INT32_MAX);       // Scale back to int32 range
}

void TapeDelay_process(TapeDelay *tapeDelay, int32_t *buf,
                       unsigned int nr_samples) {
  float previous_delay_time =
      Slew_process(&tapeDelay->delay_slew);  // Get initial delay time

  for (unsigned int i = 0; i < nr_samples; i++) {
    // Update feedback and delay time dynamically
    int32_t feedback =
        q16_16_float_to_fp(Slew_process(&tapeDelay->feedback_slew));
    float delay_time = Slew_process(&tapeDelay->delay_slew);

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
    int32_t input_sample = buf[i];
    int32_t processed_sample =
        input_sample + q16_16_multiply(feedback, delayed_sample);

    // Apply tanh-like saturation to prevent harsh distortion
    processed_sample = tanh_approx(processed_sample);

    tapeDelay->buffer[tapeDelay->write_index] = processed_sample;

    // Update write index
    tapeDelay->write_index =
        (tapeDelay->write_index + 1) % tapeDelay->buffer_size;

    // Store the processed sample back in the buffer
    buf[i] = processed_sample;
  }
}

void TapeDelay_free(TapeDelay *tapeDelay) {
  if (tapeDelay != NULL) {
    free(tapeDelay);
  }
}

#endif
