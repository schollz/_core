#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../sort.h"

int main() {
  int16_t array[] = {10, 20, 30, 40, 1, -1, -1};
  int n = sizeof(array) / sizeof(array[0]);
  uint16_t *indexes = sort_int16_t(array, n);
  // Print the sorted indexes.
  for (int i = 0; i < n; i++) {
    printf("%d) array[%d]=%d\n", i, indexes[i], array[indexes[i]]);
  }
  printf("\n");
  free(indexes);

  return 0;
}