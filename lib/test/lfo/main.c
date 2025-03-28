// Copyright 2023-2025 Zack Scholl, GPLv3.0

//  gcc -o main main.c && ./main  | gnuplot -p -e 'plot "/dev/stdin"  using 0:1'
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../fixedpoint.h"

int main() {
  int32_t x = 0;
  float hz = 2;
  int32_t increase = round(9340 * hz);
  for (int i = 0; i < 441; i++) {
    x += increase;
    float y;
    y = q16_16_fp_to_float(q16_16_cos(x));
    printf("%2.3f\n", y);
  }
  increase = round(9340 * hz * 2);
  for (int i = 0; i < 441; i++) {
    x += increase;
    float y;
    y = q16_16_fp_to_float(q16_16_cos(x));
    printf("%2.3f\n", y);
  }

  return 0;
}