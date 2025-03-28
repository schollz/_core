// Copyright 2023-2025 Zack Scholl, GPLv3.0

//  gcc -o main main.c && ./main  | gnuplot -p -e 'plot "/dev/stdin"  using 0:1'
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../envelopegate.h"

int main() {
  // Create a EnvelopeGate instance
  uint32_t sampleRate = 44100;

  EnvelopeGate *envelope2 = EnvelopeGate_create(sampleRate, 1, 0, 0.75, 1);
  for (int i = 0; i < sampleRate * 2; i++) {
    float value = EnvelopeGate_update(envelope2);
    printf("%2.3f\n", value);
  }

  EnvelopeGate_destroy(envelope2);

  return 0;
}