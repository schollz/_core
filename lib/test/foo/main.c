// Copyright 2023-2025 Zack Scholl, GPLv3.0

#include <stdint.h>
#include <stdio.h>

int main() {
  unsigned int num;
  int32_t v = 600;
  v = v * 1;
  v = v << 8;
  printf("%d\n", v);
  v = v + (v >> 16u);
  printf("%d\n", v);

  printf("%d\n", 1 << 0);
  return 0;
}