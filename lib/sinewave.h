#ifndef LIB_SINEWAVE_H
#define LIB_SINEWAVE_H 1
#include "fixedpoint.h"
//
#include "crossfade3.h"
#include "sinewaves2.h"
#include "stdbool.h"

typedef struct SinOsc {
  uint8_t wave[2];
  uint16_t phase[2];
  uint16_t limit[2];
  uint8_t quiet[2];
  float crossfade_index;
} SinOsc;

void SinOsc_wave(SinOsc *self, uint8_t wave) {
  if (self->wave[0] == wave) {
    return;
  }
  if (wave >= 0 && wave < SINEWAVE_MAX) {
    self->crossfade_index = 0;
    self->quiet[1] = self->quiet[0];
    self->wave[1] = self->wave[0];
    self->phase[1] = self->phase[0];
    self->limit[1] = self->limit[0];
    self->wave[0] = wave;
    self->phase[0] = 0;
    self->limit[0] = sinewave_len(wave);
  }
}

void SinOsc_quiet(SinOsc *self, uint8_t quiet) {
  if (quiet >= 0 && quiet <= 16) {
    self->crossfade_index = 0;
    self->quiet[1] = self->quiet[0];
    self->wave[1] = self->wave[0];
    self->phase[1] = self->phase[0];
    self->limit[1] = self->limit[0];
    self->quiet[0] = quiet;
  }
}

SinOsc *SinOsc_malloc() {
  SinOsc *self = malloc(sizeof(SinOsc));
  self->quiet[0] = 0;
  self->quiet[1] = 0;
  self->wave[0] = 0;
  self->wave[1] = 0;
  self->phase[0] = 0;
  self->phase[1] = 0;
  self->limit[0] = sinewave_len(0);
  self->limit[1] = sinewave_len(0);
  self->crossfade_index = CROSSFADE3_LIMIT;
  return self;
}

void SinOsc_free(SinOsc *self) { free(self); }

int32_t SinOsc_next(SinOsc *self) {
  int32_t val = 0;
  if (self->crossfade_index < CROSSFADE3_LIMIT) {
    if (self->phase[0] >= self->limit[0]) {
      self->phase[0] = 0;
    }
    if (self->phase[1] >= self->limit[1]) {
      self->phase[1] = 0;
    }

    if (self->wave[0] == 0) {
      int32_t crossfade_multiplier =
          crossfade3_cos_out[(int)floor(self->crossfade_index)];
      crossfade_multiplier +=
          q16_16_multiply(crossfade3_cos_out[(int)ceil(self->crossfade_index)] -
                              crossfade_multiplier,
                          q16_16_float_to_fp(self->crossfade_index -
                                             floor(self->crossfade_index)));
      fprintf(stderr, "crossfade_multiplier: %d\n", crossfade_multiplier);
      val = q16_16_multiply(crossfade_multiplier,
                            sinewave_sample(self->wave[1], self->phase[1])) >>
            self->quiet[1];
    } else if (self->wave[1] == 0) {
      int32_t crossfade_multiplier =
          crossfade3_cos_out[(int)floor(self->crossfade_index)];
      crossfade_multiplier +=
          q16_16_multiply(crossfade3_cos_out[(int)ceil(self->crossfade_index)] -
                              crossfade_multiplier,
                          q16_16_float_to_fp(self->crossfade_index -
                                             floor(self->crossfade_index)));
      val = q16_16_multiply(Q16_16_1 - crossfade_multiplier,
                            sinewave_sample(self->wave[0], self->phase[0])) >>
            self->quiet[0];
    } else {
      int32_t crossfade_multiplier =
          crossfade3_line[(int)floor(self->crossfade_index)];
      crossfade_multiplier +=
          q16_16_multiply(crossfade3_line[(int)ceil(self->crossfade_index)] -
                              crossfade_multiplier,
                          q16_16_float_to_fp(self->crossfade_index -
                                             floor(self->crossfade_index)));
      val = (q16_16_multiply(Q16_16_1 - crossfade_multiplier,
                             sinewave_sample(self->wave[0], self->phase[0])) >>
             self->quiet[0]) +
            (q16_16_multiply(crossfade_multiplier,
                             sinewave_sample(self->wave[1], self->phase[1])) >>
             self->quiet[1]);
    }
    self->phase[0]++;
    self->phase[1]++;
    if (self->wave[1] == 0) {
      self->crossfade_index += 0.5;
    } else if (self->wave[0] == 0) {
      self->crossfade_index += 0.08;
    } else {
      self->crossfade_index += 0.2;
    }
  } else if (self->wave[0] > 0) {
    if (self->phase[0] >= self->limit[0]) {
      self->phase[0] = 0;
    }
    val = sinewave_sample(self->wave[0], self->phase[0]) >> self->quiet[0];
    self->phase[0]++;
  }
  return val;
}

#endif