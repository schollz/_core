#ifndef LIB_WAVETABLEOSC_H
#define LIB_WAVETABLEOSC_H 1
#include "fixedpoint.h"
//
#include "crossfade4.h"
#include "stdbool.h"
#include "wavetable_data.h"

typedef struct WaveOsc {
  uint8_t wave;
  uint16_t phase;
  uint16_t limit;
  uint8_t quiet;
  bool fade_on[2];
  bool finished;
  uint8_t fade_len[2];
  uint16_t fade_pos[2];
  uint16_t fade_posi[2];
  const int16_t *wavetable_arr;
} WaveOsc;

WaveOsc *WaveOsc_malloc(uint8_t wave, uint8_t quiet, uint8_t attack,
                        uint8_t decay) {
  WaveOsc *self = malloc(sizeof(WaveOsc));
  self->wave = wave;
  self->wavetable_arr = wavetable_data(wave);
  self->quiet = quiet;
  self->phase = 0;
  self->limit = wavetable_len(wave);
  self->fade_on[0] = true;
  self->fade_on[1] = false;
  self->fade_len[0] = attack;
  self->fade_len[1] = decay;
  self->fade_pos[0] = 0;
  self->fade_pos[1] = 0;
  self->fade_posi[0] = 0;
  self->fade_posi[1] = 0;
  self->finished = false;
  return self;
}

void WaveOsc_free(WaveOsc *self) { free(self); }

int32_t WaveOsc_next(WaveOsc *self) {
  if (self->finished) {
    return 0;
  }
  if (self->phase >= self->limit) {
    self->phase = 0;
  }
  int32_t val = self->wavetable_arr[self->phase] >> self->quiet;
  // TODO: if quiet is changed, update it when the val is 0
  for (uint8_t i = 0; i < 2; i++) {
    if (self->fade_on[i]) {
      if (i == 0) {
        int32_t xfade = crossfade3_cos_in[self->fade_pos[i]];
        if (self->fade_posi[i] > 0 &&
            self->fade_pos[i] < CROSSFADE3_LIMIT - 1) {
          xfade += (((crossfade3_cos_in[self->fade_pos[i] + 1] - xfade) *
                     self->fade_posi[i]) /
                    self->fade_len[i]);
        }
        val = q16_16_multiply(val, xfade);
      } else {
        int32_t xfade = crossfade3_exp_out[self->fade_pos[i]];
        if (self->fade_posi[i] > 0 && self->fade_pos[i] < CROSSFADE3_LIMIT) {
          xfade += (((crossfade3_exp_out[self->fade_pos[i] + 1] - xfade) *
                     self->fade_posi[i]) /
                    self->fade_len[i]);
        }
        val = q16_16_multiply(val, xfade);
      }
      self->fade_posi[i]++;
      if (self->fade_posi[i] >= self->fade_len[i]) {
        self->fade_posi[i] = 0;
        self->fade_pos[i]++;
      }
      if (self->fade_pos[i] >= CROSSFADE3_LIMIT) {
        self->fade_on[i] = false;
        if (i == 1) {
          fprintf(stderr, "finished %d\n", self->phase);
          self->finished = true;
        }
      }
    }
  }

  self->phase++;
  return val;
}

void WaveOsc_release(WaveOsc *self) {
  if (!self->finished) {
    self->fade_on[1] = true;
  }
}

void WaveOsc_release_fast(WaveOsc *self) {
  if (self->fade_on[1] == false) {
    self->fade_len[1] = 0;
    self->fade_on[1] = true;
  }
}
#endif