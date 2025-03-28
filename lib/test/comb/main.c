// Copyright 2023-2025 Zack Scholl, GPLv3.0

#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../../comb.h"
#define NUM_SAMPLES 3000

int main() {
  int32_t samples[NUM_SAMPLES];
  for (uint32_t i = 0; i < NUM_SAMPLES / 2; i++) {
    samples[2 * i + 0] =
        (int32_t)(32767.0 * sin(2.0 * M_PI * 2000.0 * i / 44100.0));
    samples[2 * i + 1] =
        (int32_t)(32767.0 * sin(2.0 * M_PI * 2000.0 * i / 44100.0));
  }

  Comb *comb;
  comb = Comb_malloc();
  Comb_setActive(comb, true);
  Comb_process(comb, samples, NUM_SAMPLES / 2, 0);

  for (uint32_t i = 0; i < NUM_SAMPLES / 2; i++) {
    printf("%d,%d\n", i, samples[2 * i]);
  }

  Comb_free(comb);
  return 0;
}