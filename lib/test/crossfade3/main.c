// Copyright 2023-2025 Zack Scholl, GPLv3.0

#include <stdint.h>
#include <stdio.h>

#include "../../fixedpoint.h"
//
#include "../../crossfade3.h"

int main() {
  for (uint16_t i = 0; i < 441; i++) {
    printf("%d\n", crossfade3_out(10, i, 0) + crossfade3_in(50, i, 0));
  }
  return 0;
}