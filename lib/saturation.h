#ifndef LIB_SATURATION_H_
#define LIB_SATURATION_H_

#include "transfer_saturate2.h"

typedef struct Saturation {
  // crossfading repeats
  bool on;
  int16_t crossfade_in;
  int16_t crossfade_out;
} Saturation;

void Saturation_free(Saturation *self) { free(self); }

Saturation *Saturation_malloc() {
  Saturation *self = (Saturation *)malloc(sizeof(Saturation));
  self->on = false;
  self->crossfade_in = CROSSFADE3_LIMIT;
  self->crossfade_out = CROSSFADE3_LIMIT;
  return self;
}
void Saturation_setActive(Saturation *self, bool on) {
  if (on) {
    self->crossfade_in = 0;
    self->crossfade_out = CROSSFADE3_LIMIT;
  } else if (self->on) {
    self->crossfade_in = CROSSFADE3_LIMIT;
    self->crossfade_out = 0;
  }
}

void __not_in_flash_func(Saturation_process)(Saturation *self, int16_t *samples,
                                             uint16_t num_samples) {
  for (int ii = 0; ii < num_samples; ii++) {
    if (self->crossfade_in < CROSSFADE3_LIMIT) {
      samples[ii] = q16_16_fp_to_int16(
          q16_16_multiply(
              Q16_16_1 - crossfade3_line[self->crossfade_in],
              q16_16_int16_to_fp(transfer_doublesine(samples[ii]) * 3 / 4)) +
          q16_16_multiply(crossfade3_line[self->crossfade_in],
                          q16_16_int16_to_fp(samples[ii])));
      self->crossfade_in++;
      if (self->crossfade_in == CROSSFADE3_LIMIT) {
        self->on = true;
      }
    } else if (self->crossfade_out < CROSSFADE3_LIMIT) {
      samples[ii] = q16_16_fp_to_int16(
          q16_16_multiply(
              crossfade3_line[self->crossfade_out],
              q16_16_int16_to_fp(transfer_doublesine(samples[ii]) * 3 / 4)) +
          q16_16_multiply(Q16_16_1 - crossfade3_line[self->crossfade_out],
                          q16_16_int16_to_fp(samples[ii])));
      self->crossfade_out++;
      if (self->crossfade_out == CROSSFADE3_LIMIT) {
        self->on = false;
      }
    } else if (self->on) {
      samples[ii] = transfer_doublesine(samples[ii]) * 3 / 4;
    }
  }
}

#endif