#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../utils.h"

int main() {
  printf("hello world\n");
  uint16_t x = 0b0100000000000001;
  printf(BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(x));
  printf(BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(x >> 8));

  x = bit_set(x, 2, 1);
  x = bit_set(x, 15, 1);
  x = bit_set(x, 14, 0);
  printf(BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(x));
  printf(BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(x >> 8));
}