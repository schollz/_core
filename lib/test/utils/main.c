#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "../../utils.h"

void print_rhythm(bool *rhythm, int n) {
  for (int i = 0; i < n; i++) {
    printf("%c ", rhythm[i] ? 'X' : '.');
  }
  printf("\n");
}
int main() {
  uint8_t x = 24;
  printf("map(%d, 0, 255, 0, 100) = %d\n", x,
         linlin_uint8_t(x, 0, 255, 0, 100));
  printf("map(%d, 0, 30, 0, 60000) = %d\n", x,
         linlin_uint16_t(x, 0, 30, 0, 60000));
  printf("map(%d, 0, 30, 0, 888888) = %d\n", x,
         linlin_uint32_t(x, 0, 30, 0, 888888));
  printf("map(%d, 0, 255, 0, 100) = %d\n", x, linlin(x, 0, 255, 0, 100));
  printf("map(%d, 0, 30, 0, 60000) = %d\n", x, linlin(x, 0, 30, 0, 60000));
  printf("map(%d, 0, 30, 0, 888888) = %d\n", x, linlin(x, 0, 30, 0, 888888));

  int n = 16;
  int k = 8;
  bool rhythm[n];

  generate_euclidean_rhythm(n, k, (n / k) / 2 + 1, rhythm);
  print_rhythm(rhythm, n);

  return 0;
}
