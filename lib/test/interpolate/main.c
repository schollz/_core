// Copyright 2023 Zack Scholl.
//
// Author: Zack Scholl (zack.scholl@gmail.com)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// See http://creativecommons.org/licenses/MIT/ for more information.

#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../../array_resample.h"

void function_to_benchmark() {
  int16_t arr[] = {11621, 11620, 10309, 6301, 2171, 650, 2136, 4150, 4507};
  int16_t arr_size = sizeof(arr) / sizeof(arr[0]);
  int16_t arr_new_size = 80;
  for (int i = 0; i < 10000000; i++) {
    int16_t *newArray =
        array_resample_quadratic_fp(arr, arr_size, arr_new_size);
    free(newArray);
  }
}

void benchmark_function(void (*function)(), const char *function_name) {
  clock_t start, end;
  double cpu_time_used;

  start = clock();  // Record the start time

  // Call the function to benchmark
  function();

  end = clock();  // Record the end time

  cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC;
  printf("Function %s took %f seconds to execute.\n", function_name,
         cpu_time_used);
}

int main() {
  int16_t arr[] = {11621, 11620, 10309, 6301, 2171, 650, 2136, 4150, 4507};
  int16_t arr_size = sizeof(arr) / sizeof(arr[0]);
  int16_t arr_new_size = 80;
  for (int i = 0; i < arr_size; i++) {
    printf("%f,%d\n", (float)i / (float)(arr_size - 1), arr[i]);
  }

  int16_t *newArray = array_resample_linear(arr, arr_size, arr_new_size);
  for (int i = 0; i < arr_new_size; i++) {
    printf("%f,%d\n", (float)i / (float)(arr_new_size - 1), newArray[i]);
  }
  newArray = array_resample_linear2(arr, arr_size, arr_new_size);
  for (int i = 0; i < arr_new_size; i++) {
    printf("%f,%d\n", (float)i / (float)(arr_new_size - 1), newArray[i]);
  }
  newArray = array_resample_quadratic_fp(arr, arr_size, arr_new_size);
  for (int i = 0; i < arr_new_size; i++) {
    printf("%f,%d\n", (float)i / (float)(arr_new_size - 1), newArray[i]);
  }

  int16_t *interpolated_array =
      hermite_interpolation(arr, arr_size, arr_new_size);
  for (int i = 0; i < arr_new_size; i++) {
    printf("%f,%d\n", (float)i / (float)(arr_new_size - 1),
           interpolated_array[i]);
  }

  free(newArray);

  // benchmark_function(function_to_benchmark, "function_to_benchmark");

  return 0;
}