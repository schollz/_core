// Copyright 2023-2025 Zack Scholl, GPLv3.0

//  gcc -o main main.c && ./main  | gnuplot -p -e 'plot "/dev/stdin"  using 0:1'
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../envelope1.h"

int main() {
  // Create a Envelope1 instance
  uint32_t sampleRate = 44100;
  Envelope1 *envelope1 = Envelope1_create(sampleRate, 0, 8000, 44100);

  for (int i = 0; i < 44100; i++) {
    int32_t value = Envelope1_update(envelope1);
    printf("%d\n", value);
  }

  envelope1 = Envelope1_create(sampleRate, 0, 5241, 4000);
  for (int i = 0; i < 44100; i++) {
    int32_t value = Envelope1_update(envelope1);
    printf("%d\n", value);
  }

  envelope1 = Envelope1_create(sampleRate, 5241, 0, 4000);
  for (int i = 0; i < 44100; i++) {
    int32_t value = Envelope1_update(envelope1);
    printf("%d\n", value);
  }

  Envelope1_destroy(envelope1);

  return 0;
}