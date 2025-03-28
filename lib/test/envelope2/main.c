// Copyright 2023-2025 Zack Scholl, GPLv3.0

//  gcc -o main main.c && ./main  | gnuplot -p -e 'plot "/dev/stdin"  using 0:1'
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../envelope2.h"

int main() {
  // Create a Envelope2 instance
  uint32_t sampleRate = 44100;

  Envelope2 *envelope2 = Envelope2_create(sampleRate, 3, 20, 2);
  for (int i = 0; i < 88200; i++) {
    float value = Envelope2_update(envelope2);
    printf("%2.3f\n", value);
  }
  envelope2 = Envelope2_create(sampleRate, 20, 0.01, 1);
  for (int i = 0; i < 44100; i++) {
    float value = Envelope2_update(envelope2);
    printf("%2.3f\n", value);
  }

  Envelope2_destroy(envelope2);

  return 0;
}