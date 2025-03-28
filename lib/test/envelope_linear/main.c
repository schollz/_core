// Copyright 2023-2025 Zack Scholl, GPLv3.0

//  gcc -o main main.c && ./main  | gnuplot -p -e 'plot "/dev/stdin"  using 0:1'
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../envelope_linear_integer.h"

int32_t val;
void env_changed_callback(int32_t v) { val = v; }

int main() {
  // Create a EnvelopeLinearInteger instance
  uint32_t sampleRate = 500;
  EnvelopeLinearInteger *envelope1 =
      EnvelopeLinearInteger_create(sampleRate, 30, -30, 0.9);

  for (int i = 0; i < sampleRate; i++) {
    EnvelopeLinearInteger_update(envelope1, env_changed_callback);
    printf("%d\n", val);
  }

  EnvelopeLinearInteger_destroy(envelope1);

  return 0;
}