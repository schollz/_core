// Copyright 2023-2025 Zack Scholl, GPLv3.0

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include "../../fixedpoint.h"

int main() {
  int32_t a = q16_16_int32_to_fp(-80000);
  int32_t b = q16_16_float_to_fp(-0.25);
  printf("a * b = %d\n", q16_16_fp_to_int32(q16_16_multiply(a, b)));

  int32_t v = -75000;

  printf("%d\n", q16_16_multiply(v, q16_16_float_to_fp(0.5123)));

  int32_t start = 0;
  for (int32_t i = start; i < start + Q16_16_2PI; i += Q16_16_2PI / 96) {
    int32_t l = q16_16_multiply(v, q16_16_sin01(i));
    int32_t r = q16_16_multiply(v, Q16_16_1 - q16_16_sin01(i));
    printf("%d %d\n", l, r);
  }

  return 0;
}