// Copyright 2023-2025 Zack Scholl, GPLv3.0

//  gcc -o main main.c && ./main  | gnuplot -p -e 'plot "/dev/stdin"  using 0:1'
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../envelope3.h"

int main() {
  // Create a Envelope3 instance
  uint32_t sampleRate = 44100;

  Envelope3 *envelope3;

  envelope3 = Envelope3_create(sampleRate, 0.1, 1.1, 1.1, 0.0, 0.5, 0.6, 0.9);
  for (int i = 0; i < 88200; i++) {
    float value = Envelope3_update(envelope3);
    printf("%2.3f\n", value);
  }
  Envelope3_destroy(envelope3);

  return 0;
}