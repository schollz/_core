// Copyright 2023-2025 Zack Scholl, GPLv3.0

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../delay.h"
#include "../../sinewave.h"

int main() {
  init_sinewaves();
  SinOsc *osc1 = SinOsc_malloc();
  SinOsc *osc2 = SinOsc_malloc();
  SinOsc_wave(osc1, 24);
  SinOsc_quiet(osc1, 0);
  SinOsc_wave(osc2, 29);
  SinOsc_quiet(osc2, 1);
  Delay *dly = Delay_malloc();
  Delay_setDuration(dly, 1000);
  Delay_setActive(dly, true);
  int32_t vals[8000];
  for (int i = 0; i < 8000; i++) {
    if (i < 500) {
      vals[i] = (SinOsc_next(osc1) >> 17) + (SinOsc_next(osc2) >> 17);
    } else {
      vals[i] = 0;
    }
  }
  Delay_process(dly, vals, 8000);
  for (int i = 0; i < 8000; i++) {
    printf("%d\n", vals[i]);
  }

  SinOsc_free(osc1);
  SinOsc_free(osc2);
  Delay_free(dly);
  return 0;
}