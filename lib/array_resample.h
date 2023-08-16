#define INTERPOLATE_VALUE 512
int16_t *array_resample_linear(int16_t *arr, int16_t arr_size,
                               int16_t newSize) {
  int16_t *newArray;
  newArray = malloc(sizeof(int16_t) * newSize);

  uint32_t stepSize = (arr_size - 1) * INTERPOLATE_VALUE / (newSize - 1);

  for (int16_t i = 0; i < newSize; i++) {
    uint32_t index = (i * stepSize) / INTERPOLATE_VALUE;
    int32_t frac = (i * stepSize) - (index * INTERPOLATE_VALUE);
    // printf("stepSize: %d, i: %d, index: %d, frac: %d, arr[index]: %d\n",
    //        stepSize, i, index, frac, arr[index]);
    if (index < arr_size - 1) {
      int32_t x = ((int32_t)arr[index] * (INTERPOLATE_VALUE - frac)) +
                  ((int32_t)arr[index + 1] * frac);
      // round to nearest interpolate value
      // printf("%d, x: %d\n", i, x);
      int16_t round_int = x / INTERPOLATE_VALUE;
      if (x % INTERPOLATE_VALUE > INTERPOLATE_VALUE / 2) {
        round_int++;
      }
      newArray[i] = round_int;
    } else {
      newArray[i] = arr[arr_size - 1];
    }
  }
  return newArray;
}
