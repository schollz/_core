#ifndef LIB_WAVETABLEOSC_H
#define LIB_WAVETABLEOSC_H 1
#include "fixedpoint.h"
//
#include "crossfade3.h"
#include "stdbool.h"
#include "wavetable_data.h"

typedef struct WaveOsc {
  uint8_t wave;
  uint16_t phase;
  uint16_t limit;
  uint8_t quiet;
  bool fade_in;
  bool fade_out;
  uint16_t crossfade;
  uint8_t crossfade_factor;
} WaveOsc;

WaveOsc *WaveOsc_malloc(uint8_t wave, uint8_t quiet) {
  WaveOsc *self = malloc(sizeof(WaveOsc));
  self->wave = wave;
  self->quiet = quiet;
  self->phase = 0;
  self->limit = wavetable_len(wave);
  self->fade_in = true;
  self->fade_out = false;
  self->crossfade = 0;
  self->crossfade_factor = 0;
  return self;
}

void WaveOsc_free(WaveOsc *self) { free(self); }

int32_t WaveOsc_next(WaveOsc *self) {
  int32_t val = 0;

  if (self->crossfade < CROSSFADE3_LIMIT) {
    if (self->phase[0] >= self->limit[0]) {
      self->phase[0] = 0;
    }
    if (self->phase[1] >= self->limit[1]) {
      self->phase[1] = 0;
    }
    if (self->wave[0] == 0) {
      val = q16_16_multiply(crossfade3_cos_out[self->crossfade],
                            wavetable_sample(self->wave[1], self->phase[1])) >>
            self->quiet[1];
      if (self->crossfade_factor == 0) {
        self->crossfade++;
        self->crossfade_factor = 2;
      } else {
        self->crossfade_factor--;
      }
    } else if (self->wave[1] == 0) {
      val = q16_16_multiply(Q16_16_1 - crossfade3_line[self->crossfade],
                            wavetable_sample(self->wave[0], self->phase[0])) >>
            self->quiet[0];

      self->crossfade++;
    } else {
      val = (q16_16_multiply(Q16_16_1 - crossfade3_line[self->crossfade],
                             wavetable_sample(self->wave[0], self->phase[0])) >>
             self->quiet[0]) +
            (q16_16_multiply(crossfade3_line[self->crossfade],
                             wavetable_sample(self->wave[1], self->phase[1])) >>
             self->quiet[1]);
      self->crossfade++;
    }
    self->phase[0]++;
    self->phase[1]++;
  } else if (self->wave[0] > 0) {
    if (self->phase[0] >= self->limit[0]) {
      self->phase[0] = 0;
    }
    val = wavetable_sample(self->wave[0], self->phase[0]) >> self->quiet[0];
    self->phase[0]++;
  }
  return val;
}

#endif