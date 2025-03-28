// Copyright 2023-2025 Zack Scholl, GPLv3.0

#include <stdint.h>
#include <stdio.h>

int16_t wavefold(int16_t x, int16_t v) {
  if (x == 0) {
    return x;
  }
  int32_t x2 = x * v / 100;
  if (x2 > 32767) {
    return 32767 - (x2 - 32767);
  } else if (x2 < -32767) {
    return -32767 - (x2 + 32767);
  }
  return x;
}

int main() {
  printf("wavefold: %d\n", wavefold(32000, 150));
  printf("wavefold: %d\n", wavefold(-19999, 150));
  return 0;
}