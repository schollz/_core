// Copyright 2023-2025 Zack Scholl, GPLv3.0

//  gcc -o main main.c && ./main  | gnuplot -p -e 'plot "/dev/stdin"  using 0:1
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../fixedpoint.h"

int main() {
  // printf("%d\n", q16_16_float_to_fp(2 * 3.14159));
  // printf("%d\n", q16_16_float_to_fp(32767));
  for (int64_t i = -1 * INT32_MAX; i < INT32_MAX; i += INT32_MAX / 1000) {
    printf("%d\n",
           q16_16_sin(q16_16_multiply(Q16_16_2PI, q16_16_divide(i, Q16_16_MAX)))
               << 15);
  }

  return 0;
}