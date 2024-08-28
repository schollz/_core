#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "../../utils.h"

void print_rhythm(bool *rhythm, int n) {
  for (int i = 0; i < n; i++) {
    printf("%d ", rhythm[i]);
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
  int k = 7;
  bool rhythm[n];

  generate_euclidean_rhythm(n, k, 0, rhythm);
  uint8_t arr[n];

  print_rhythm(rhythm, n);

  uint8_t total = 0;
  for (int i = 0; i < n; i++) {
    arr[i] = rhythm[i];
    total += arr[i];
  }
  if (total == 0) {
    return;
  }

  // rotate until i=0 has a 1 in it
  while (arr[0] == 0) {
    bool tmp = arr[n - 1];
    for (int j = n - 1; j > 0; j--) {
      arr[j] = arr[j - 1];
    }
    arr[0] = tmp;
  }

  // cumulative sum of arr
  arr[0] = 0;
  for (int i = 1; i < n; i++) {
    arr[i] += arr[i - 1];
  }

  // print out arr
  for (int i = 0; i < n; i++) {
    printf("%d ", arr[i]);
  }

  return 0;
}
