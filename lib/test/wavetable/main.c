// Copyright 2023-2025 Zack Scholl, GPLv3.0

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../fixedpoint.h"
#include "../../wavetableosc.h"

int main() {
  WaveOsc *osc1 = WaveOsc_malloc(18, 3, 7, 15);
  for (int i = 0; i < 4000; i++) {
    printf("%d\n", WaveOsc_next(osc1));
  }
  // WaveOsc_release(osc1);
  // for (int i = 0; i < 8000; i++) {
  //   printf("%d\n", WaveOsc_next(osc1));
  // }
  WaveOsc_free(osc1);

  // osc1 = WaveOsc_malloc(21, 3, 12, 4);
  // for (int i = 0; i < 4000; i++) {
  //   printf("%d\n", WaveOsc_next(osc1));
  // }
  // WaveOsc_release_fast(osc1);
  // for (int i = 0; i < 4000; i++) {
  //   printf("%d\n", WaveOsc_next(osc1));
  // }
  // WaveOsc_free(osc1);
  return 0;
}