#ifndef SORT_LIB
#define SORT_LIB

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

#endif