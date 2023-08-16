#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../../array_resample.h"

int main() {
  int16_t arr[] = {11620, 10309, 6301, 2171, 650, 2136, 4150, 4507};
  int16_t arr_size = sizeof(arr) / sizeof(arr[0]);
  int16_t arr_new_size = 4;
  int16_t *newArray = array_resample_linear(arr, arr_size, arr_new_size);
  for (int i = 0; i < arr_new_size; i++) {
    printf("%f,%d\n", (float)i / (float)(arr_new_size - 1), newArray[i]);
  }
  for (int i = 0; i < arr_size; i++) {
    printf("%f,%d\n", (float)i / (float)(arr_size - 1), arr[i]);
  }

  free(newArray);
  return 0;
}