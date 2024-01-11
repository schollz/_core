#ifndef LIB_WAVETABLEBASS_H
#define LIB_WAVETABLEBASS_H 1
#include "wavetablesyn.h"
#define WAVETABLEBASS_MAX 3

const uint8_t wavetablebass_harmonics[3] = {0, 12, 19};
typedef struct WaveBass {
  WaveSyn *osc[WAVETABLEBASS_MAX];
} WaveBass;

WaveBass *WaveBass_malloc() {
  WaveBass *self = malloc(sizeof(WaveBass));
  for (uint8_t i = 0; i < WAVETABLEBASS_MAX; i++) {
    self->osc[i] = WaveSyn_malloc();
  }
  return self;
}

void WaveBass_free(WaveBass *self) {
  for (uint8_t i = 0; i < WAVETABLEBASS_MAX; i++) {
    WaveSyn_free(self->osc[i]);
  }
  free(self);
}

void WaveBass_note_on(WaveBass *self, uint8_t note) {
  for (uint8_t i = 0; i < WAVETABLEBASS_MAX; i++) {
    WaveSyn_new(self->osc[i], note + wavetablebass_harmonics[i], 1 + i, 5, 100);
  }
}

void WaveBass_release(WaveBass *self) {
  for (uint8_t i = 0; i < WAVETABLEBASS_MAX; i++) {
    WaveSyn_release(self->osc[i]);
  }
}

int32_t WaveBass_next(WaveBass *self) {
  int64_t val = 0;
  for (uint8_t i = 0; i < WAVETABLEBASS_MAX; i++) {
    val += WaveSyn_next(self->osc[i]);
  }
  return val / WAVETABLEBASS_MAX;
}

#endif