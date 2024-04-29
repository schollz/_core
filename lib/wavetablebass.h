#ifndef LIB_WAVETABLEBASS_H
#define LIB_WAVETABLEBASS_H 1
#include "wavetablesyn.h"
#define WAVETABLEBASS_MAX 3

const uint8_t wavetablebass_harmonics[3] = {0, 12, 19};
typedef struct WaveBass {
  WaveSyn *osc[WAVETABLEBASS_MAX];
  uint8_t note;
  uint16_t change_count;
  uint16_t volume;
} WaveBass;

WaveBass *WaveBass_malloc() {
  WaveBass *self = malloc(sizeof(WaveBass));
  for (uint8_t i = 0; i < WAVETABLEBASS_MAX; i++) {
    self->osc[i] = WaveSyn_malloc();
  }
  self->note = 0;
  self->change_count = 5000;
  self->volume = 4096 / 4;
  return self;
}

void WaveBass_free(WaveBass *self) {
  for (uint8_t i = 0; i < WAVETABLEBASS_MAX; i++) {
    WaveSyn_free(self->osc[i]);
  }
  free(self);
}

void WaveBass_set_volume(WaveBass *self, uint16_t volume) {
  if (volume > 4096) {
    volume = 4096;
  }
  self->volume = volume;
}

void WaveBass_note_on(WaveBass *self, uint8_t note) {
  self->change_count = 0;
  self->note = note;
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
  if (self->change_count < 2000) {
    if (self->change_count == 1) {
      WaveSyn_new(self->osc[0], self->note + wavetablebass_harmonics[0], 0, 1,
                  100);
    } else if (self->change_count == 500 && WAVETABLEBASS_MAX > 1) {
      WaveSyn_new(self->osc[1], self->note + wavetablebass_harmonics[1], 1, 3,
                  50);
    } else if (self->change_count == 1000 && WAVETABLEBASS_MAX > 2) {
      WaveSyn_new(self->osc[2], self->note + wavetablebass_harmonics[2], 2, 4,
                  25);
    }
    self->change_count++;
  }
  return (val << 4) * self->volume / WAVETABLEBASS_MAX;
}

#endif