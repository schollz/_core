#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

uint16_t *sort_int16_t(int16_t array[], int n) {
  // Create an auxiliary array to store the indexes.
  uint16_t *indexes = malloc(sizeof(uint16_t) * n);
  for (int i = 0; i < n; i++) {
    indexes[i] = i;
  }

  // Sort the auxiliary array based on the values in the original array.
  for (int i = 0; i < n - 1; i++) {
    for (int j = i + 1; j < n; j++) {
      if (array[indexes[i]] > array[indexes[j]]) {
        // Swap the elements at indexes[i] and indexes[j].
        int temp = indexes[i];
        indexes[i] = indexes[j];
        indexes[j] = temp;
      }
    }
  }

  return indexes;
}

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