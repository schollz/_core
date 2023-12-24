#ifndef LIB_SINEWAVE_H
#define LIB_SINEWAVE_H 1
#include "crossfade3.h"
#include "sinewaves.h"
#include "stdbool.h"

typedef struct SinOsc {
  uint8_t wave[2];
  uint16_t phase[2];
  uint16_t limit[2];
  uint16_t crossfade;
} SinOsc;

void SinOsc_new(SinOsc *self, uint8_t wave) {
  if (self->wave[0] == wave) {
    return;
  }
  if (wave >= 0 && wave < 16) {
    self->crossfade = 0;
    self->wave[1] = self->wave[0];
    self->phase[1] = self->phase[0];
    self->limit[1] = self->limit[0];
    self->wave[0] = wave;
    self->phase[0] = 0;
    self->limit[0] = sinewave_len(wave);
  }
}

SinOsc *SinOsc_malloc() {
  SinOsc *self = malloc(sizeof(SinOsc));
  self->wave[0] = 0;
  self->wave[1] = 0;
  self->phase[0] = 0;
  self->phase[1] = 0;
  self->limit[0] = sinewave_len(0);
  self->limit[1] = sinewave_len(0);
  self->crossfade = CROSSFADE3_LIMIT;
  return self;
}

int32_t SinOsc_next(SinOsc *self, int32_t volume) {
  int32_t val;
  if (self->crossfade < CROSSFADE3_LIMIT) {
    val = q16_16_multiply(Q16_16_1 - crossfade3_line[self->crossfade],
                          sinewave_sample(self->wave[0], self->phase[0])) +
          q16_16_multiply(crossfade3_line[self->crossfade],
                          sinewave_sample(self->wave[1], self->phase[1]));
    self->phase[0]++;
    if (self->phase[0] >= self->limit[0]) {
      self->phase[0] = 0;
    }
    self->phase[1]++;
    if (self->phase[1] >= self->limit[1]) {
      self->phase[1] = 0;
    }
    self->crossfade++;
  } else {
    val = sinewave_sample(self->wave[0], self->phase[0]);
    self->phase[0]++;
    if (self->phase[0] >= self->limit[0]) {
      self->phase[0] = 0;
    }
  }
  return q16_16_multiply(volume, val);
}

#endif