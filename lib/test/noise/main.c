// Copyright 2023-2025 Zack Scholl, GPLv3.0

//  gcc -o main main.c && ./main  | gnuplot -p -e 'plot "/dev/stdin"  using 0:1
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../noise.h"

int main() {
  // Seed for random number generation (you can change this to any other value)
  uint32_t seed = 1234;
  uint32_t sampleRate = 44100;
  // Create a Noise instance
  Noise *noise = Noise_create(seed, sampleRate);

  // Generate and print some random noise using LFNoise0
  int32_t frequency = 10;  // Frequency in Hz
  for (int i = 0; i < sampleRate * 4; i++) {
    float value = LFNoise0(noise, frequency);
    printf("%f\n", value);
  }

  for (int i = 0; i < sampleRate * 4; i++) {
    float value = LFNoise0_seeded(noise, frequency, 8, seed);
    printf("%d\n", (uint8_t)Range(value, 0, 16));
  }

  for (int i = 0; i < sampleRate * 4; i++) {
    float value = LFNoise2(noise, frequency);
    printf("%f\n", (float)Range(value, 0.8, 1.2));
  }

  // Don't forget to free the memory when you're done using the Noise struct
  Noise_destroy(noise);

  return 0;
}