// Copyright 2023-2025 Zack Scholl, GPLv3.0

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../fixedpoint.h"
#include "../../sinewave.h"

int main() {
  SinOsc *osc1 = SinOsc_malloc();
  // SinOsc *osc2 = SinOsc_malloc();
  SinOsc_quiet(osc1, 0);
  SinOsc_wave(osc1, 12);
  for (int i = 0; i < 4000; i++) {
    printf("%d\n", SinOsc_next(osc1));
  }
  SinOsc_wave(osc1, 24);
  for (int i = 0; i < 4000; i++) {
    printf("%d\n", SinOsc_next(osc1));
  }
  SinOsc_wave(osc1, 0);
  for (int i = 0; i < 8000; i++) {
    printf("%d\n", SinOsc_next(osc1));
  }
  SinOsc_wave(osc1, 23);
  for (int i = 0; i < 8000; i++) {
    printf("%d\n", SinOsc_next(osc1));
  }
  // SinOsc_wave(osc1, 0);
  // for (int i = 0; i < 441; i++) {
  //   printf("%d\n", SinOsc_next(osc1));
  // }
  // SinOsc_free(osc1);
  // SinOsc_free(osc2);

  return 0;
}