#ifndef LIB_WAVETABLESYN_H
#define LIB_WAVETABLESYN_H 1
#include "wavetableosc.h"

#define WAVETABLESYN_MAX 3

typedef struct WaveSyn {
  WaveOsc *osc[WAVETABLESYN_MAX];
  bool active[WAVETABLESYN_MAX];
  uint8_t current;
} WaveSyn;

WaveSyn *WaveSyn_malloc() {
  WaveSyn *self = malloc(sizeof(WaveSyn));
  for (uint8_t i = 0; i < WAVETABLESYN_MAX; i++) {
    self->osc[i] = NULL;
    self->active[i] = false;
  }
  self->current = 0;
  return self;
}

void WaveSyn_free(WaveSyn *self) {
  for (uint8_t i = 0; i < WAVETABLESYN_MAX; i++) {
    if (self->osc[i] != NULL) {
      WaveOsc_free(self->osc[i]);
    }
  }
  free(self);
}

void WaveSyn_new(WaveSyn *self, uint8_t wave, uint8_t quiet, uint8_t attack,
                 uint8_t decay) {
  // release all the oscillators
  for (uint8_t i = 0; i < WAVETABLESYN_MAX; i++) {
    if (self->active[i]) {
      // printf("[wavetablesyn] release %d\n", i);
      WaveOsc_release_fast(self->osc[i]);
    }
  }
  // find an non-active one and activate it
  for (uint8_t i = 0; i < WAVETABLESYN_MAX; i++) {
    if (!self->active[i] && self->osc[i] == NULL) {
      self->osc[i] = WaveOsc_malloc(wave, quiet, attack, decay);
      self->active[i] = true;
      self->current = i;
      break;
    }
  }
}

int32_t WaveSyn_next(WaveSyn *self) {
  int64_t val = 0;
  for (uint8_t i = 0; i < WAVETABLESYN_MAX; i++) {
    if (self->active[i]) {
      val += WaveOsc_next(self->osc[i]);
      if (self->osc[i]->finished) {
        WaveOsc_free(self->osc[i]);
        self->osc[i] = NULL;
        self->active[i] = false;
      }
    }
  }
  if (val > 0x7fffffff) {
    val = 0x7fffffff;
  } else if (val < -0x7fffffff) {
    val = -0x7fffffff;
  }
  return val;
}

void WaveSyn_release(WaveSyn *self) {
  // releases the current
  if (self->active[self->current] && self->osc[self->current] != NULL) {
    WaveOsc_release(self->osc[self->current]);
  }
}

#endif