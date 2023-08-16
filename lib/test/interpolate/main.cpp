// g++ -o interpolate main.cpp && ./interpolate
#include <math.h>

// used for resampling

#define INTERPOLATE_VALUE 1024 * 2

// used for resampling
int16_t *interpolateArray(int16_t *arr, int16_t arr_size, int16_t newSize) {
  // int16_t *newArray = new int16_t[newSize];
  int16_t *newArray;
  newArray = malloc(sizeof(int16_t) * newSize);

  uint32_t stepSize = (arr_size - 1) * INTERPOLATE_VALUE / (newSize - 1);

  for (int16_t i = 0; i < newSize; i++) {
    uint32_t index = (i * stepSize) / INTERPOLATE_VALUE;
    uint32_t frac = (i * stepSize) - (index * INTERPOLATE_VALUE);
    // printf("stepSize: %d, i: %d, index: %d, frac: %d, arr[index]: %d\n",
    //        stepSize, i, index, frac, arr[index]);
    if (index < arr_size - 1) {
      newArray[i] =
          (arr[index] * (INTERPOLATE_VALUE - frac) + arr[index + 1] * frac);
      // round to nearest
      int round_int = newArray[i] / INTERPOLATE_VALUE;
      if (newArray[i] % INTERPOLATE_VALUE > INTERPOLATE_VALUE / 2) {
        round_int++;
      }
      newArray[i] = round_int;
    } else {
      newArray[i] = arr[arr_size - 1];
    }
  }
  return newArray;
}

int main() {
  int16_t arr[] = {1, 10, 50, 25, -50, -25, -10, 10};
  int16_t arr_size = sizeof(arr) / sizeof(arr[0]);
  int16_t arr_new_size = 24;
  int16_t *newArray = interpolateArray(arr, arr_size, arr_new_size);
  printf("array resized %d -> %d\n", arr_size, arr_new_size);
  for (int i = 0; i < arr_new_size; i++) {
    printf("%d, ", newArray[i]);
  }

  free(newArray);
  return 0;
}