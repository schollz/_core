// Copyright 2023-2025 Zack Scholl, GPLv3.0

//  gcc -o main main.c && ./main  | gnuplot -p -e 'plot "/dev/stdin"  using 0:1
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../noise.h"
#include "../../resonantfilter.h"

int main() {
  // Seed for random number generation (you can change this to any other value)
  uint32_t seed = 1234;
  uint32_t sampleRate = 44100;
  // Create a Noise instance
  Noise *noise = Noise_create(seed, sampleRate);
  ResonantFilter *rf = ResonantFilter_create(0);
  ResonantFilter_setFc(rf, 45);
  ResonantFilter_setQ(rf, 0);

  // Generate and print some random noise using LFNoise0
  int32_t frequency = 200;  // Frequency in Hz
  uint8_t v = 0;
  for (int i = 0; i < sampleRate * 0.1; i++) {
    v++;
    // printf("%d\n", v);

    printf("%d\n", ResonantFilter_update(rf, v));
  }

  // Don't forget to free the memory when you're done using the Noise struct
  Noise_destroy(noise);

  return 0;
}