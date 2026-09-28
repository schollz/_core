#ifndef SAMPLE_CV_H
#define SAMPLE_CV_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
  SAMPLE_CV_MAPPING_BANK = 0,
  SAMPLE_CV_MAPPING_1VOCT,
} SampleCVMapping;

typedef struct {
  int16_t note;
  bool valid;
  bool bipolar;
} SampleCVState;

static inline void sample_cv_reset(SampleCVState *state) {
  state->valid = false;
}

static inline void sample_cv_update(SampleCVState *state, int16_t raw_adc,
                                    bool bipolar) {
  // The existing input scale is 0 V at 512, with 512 counts per 5 V.
  if (raw_adc < 0) raw_adc = 0;
  if (raw_adc > 1023) raw_adc = 1023;
  int32_t centered = raw_adc - 512;
  if (!bipolar && centered < 0) centered = 0;
  int32_t scaled = centered * 60;  // Semitones in units of 1/512.
  int32_t delta = scaled - state->note * 512;
  // Half a semitone (256) plus one ADC count (60), in both directions.
  if (!state->valid || state->bipolar != bipolar || delta > 316 ||
      delta < -316) {
    state->note = scaled < 0 ? -((-scaled + 256) / 512) : (scaled + 256) / 512;
  }
  state->valid = true;
  state->bipolar = bipolar;
}

static inline bool sample_cv_index(const SampleCVState *state,
                                   uint16_t sample_count, uint8_t *sample) {
  if (!state->valid || sample_count == 0) return false;
  // C's negative remainder must be normalized before converting to unsigned.
  int32_t index = state->note % (int32_t)sample_count;
  if (index < 0) index += sample_count;
  *sample = (uint8_t)index;
  return true;
}

#endif
