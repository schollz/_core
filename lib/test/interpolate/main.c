// Copyright 2023-2024 Zack Scholl.
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

#include "../../fixedpoint.h"

//
#include "../../array_resample.h"

void array_resample_quadratic_fp_benchmark() {
  int16_t arr[] = {11621, 11620, 10309, 6301,  11621, 11620, 10309,
                   6301,  11621, 11620, 10309, 6301,  11621, 11620,
                   10309, 6301,  2171,  650,   2136,  4150,  4507};
  int16_t arr_size = sizeof(arr) / sizeof(arr[0]);
  int16_t arr_new_size = 80;
  for (int i = 0; i < 1000000; i++) {
    int16_t *newArray =
        array_resample_quadratic_fp(arr, arr_size, arr_new_size);
    free(newArray);
  }
}

void array_resample_quadratic_benchmark() {
  int16_t arr[] = {11621, 11620, 10309, 6301,  11621, 11620, 10309,
                   6301,  11621, 11620, 10309, 6301,  11621, 11620,
                   10309, 6301,  2171,  650,   2136,  4150,  4507};
  int16_t arr_size = sizeof(arr) / sizeof(arr[0]);
  int16_t arr_new_size = 80;
  for (int i = 0; i < 1000000; i++) {
    int16_t *newArray = array_resample_quadratic(arr, arr_size, arr_new_size);
    free(newArray);
  }
}
void array_resample_linear_benchmark() {
  int16_t arr[] = {11621, 11620, 10309, 6301,  11621, 11620, 10309,
                   6301,  11621, 11620, 10309, 6301,  11621, 11620,
                   10309, 6301,  2171,  650,   2136,  4150,  4507};
  int16_t arr_size = sizeof(arr) / sizeof(arr[0]);
  int16_t arr_new_size = 80;
  for (int i = 0; i < 1000000; i++) {
    int16_t *newArray = array_resample_linear(arr, arr_size, arr_new_size);
    free(newArray);
  }
}

void array_resample_linear_old_benchmark() {
  int16_t arr[] = {11621, 11620, 10309, 6301,  11621, 11620, 10309,
                   6301,  11621, 11620, 10309, 6301,  11621, 11620,
                   10309, 6301,  2171,  650,   2136,  4150,  4507};
  int16_t arr_size = sizeof(arr) / sizeof(arr[0]);
  int16_t arr_new_size = 80;
  for (int i = 0; i < 1000000; i++) {
    int16_t *newArray = array_resample_linear_old(arr, arr_size, arr_new_size);
    free(newArray);
  }
}
void array_resample_linear2_benchmark() {
  int16_t arr[] = {11621, 11620, 10309, 6301,  11621, 11620, 10309,
                   6301,  11621, 11620, 10309, 6301,  11621, 11620,
                   10309, 6301,  2171,  650,   2136,  4150,  4507};
  int16_t arr_size = sizeof(arr) / sizeof(arr[0]);
  int16_t arr_new_size = 80;
  for (int64_t i = 0; i < 1000000; i++) {
    int16_t *newArray = array_resample_linear2(arr, arr_size, arr_new_size);
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
  int iterations = 10;
  float freq = 100;
  int points_per_segment = 40;
  float factor = 2.8f;
  float t_inc = 2 * M_PI / points_per_segment * 1.3;
  int x = -1;
  float t = -1 * t_inc;
  for (uint8_t k = 0; k < iterations; k++) {
    int16_t arr[points_per_segment];
    for (int i = 0; i < points_per_segment; i++) {
      x++;
      t += t_inc;
      arr[i] = (int16_t)(sin(t) * freq);
      printf("%d,%d\n", x, arr[i]);
    }
  }

  x = -1;
  t = -1 * t_inc;
  for (uint8_t k = 0; k < iterations; k++) {
    int16_t arr[points_per_segment + 1];
    for (int i = 0; i < points_per_segment + 1; i++) {
      t += t_inc;
      arr[i] = (int16_t)(sin(t) * freq);
    }
    t -= t_inc;  // remove the peek
    int new_size = points_per_segment * factor;
    int16_t *newArray =
        array_resample_linear(arr, points_per_segment, new_size);
    for (int i = 0; i < new_size; i++) {
      x++;
      printf("%d,%d\n", x, newArray[i]);
    }
  }
  x = -1;
  t = -1 * t_inc;
  for (uint8_t k = 0; k < iterations; k++) {
    int16_t arr[points_per_segment + 1];
    for (int i = 0; i < points_per_segment + 1; i++) {
      t += t_inc;
      arr[i] = (int16_t)(sin(t) * freq);
    }
    t -= t_inc;  // remove the peek
    int new_size = points_per_segment * factor;
    int16_t *newArray =
        array_resample_linear_old(arr, points_per_segment, new_size);
    for (int i = 0; i < new_size; i++) {
      x++;
      printf("%d,%d\n", x, newArray[i]);
    }
  }
  // j = 0;
  // for (uint8_t k = 0; k < 3; k++) {
  //   newArray = array_resample_linear(arr, arr_size, arr_new_size);
  //   for (int i = 0; i < arr_new_size; i++) {
  //     printf("%f,%d\n", j / factor, newArray[i]);
  //     j++;
  //   }
  // }
  // j = 0;
  // for (uint8_t k = 0; k < 3; k++) {
  //   newArray = array_resample_linear_old(arr, arr_size, arr_new_size);
  //   for (int i = 0; i < arr_new_size; i++) {
  //     printf("%f,%d\n", j / factor, newArray[i]);
  //     j++;
  //   }
  // }
  // newArray = array_resample_quadratic_fp(arr, arr_size, arr_new_size);
  // for (int i = 0; i < arr_new_size; i++) {
  //   printf("%f,%d\n", (float)i / (float)(arr_new_size - 1) / 2.0f, arr[i]);
  // }
  // newArray = array_resample_quadratic_fp(arr, arr_size, arr_new_size);
  // for (int i = 0; i < arr_size; i++) {
  //   printf("%f,%d\n",
  //          (float)i / (float)(arr_new_size - 1) / 2.0f + 0.5 +
  //              (float)0.5f / (float)(arr_new_size - 1),
  //          arr[i]);
  // }

  // newArray = array_resample_linear2(arr, arr_size, arr_new_size);
  // for (int i = 0; i < arr_new_size; i++) {
  //   printf("%f,%d\n", (float)i / (float)(arr_new_size - 1), newArray[i]);
  // }

  // int16_t *interpolated_array =
  //     hermite_interpolation(arr, arr_size, arr_new_size);
  // for (int i = 0; i < arr_new_size; i++) {
  //   printf("%f,%d\n", (float)i / (float)(arr_new_size - 1),
  //          interpolated_array[i]);
  // }

  // free(newArray);

  // benchmark_function(array_resample_quadratic_fp_benchmark,
  //                    "array_resample_quadratic_fp_benchmark");
  // benchmark_function(array_resample_quadratic_benchmark,
  //                    "array_resample_quadratic_benchmark");
  // benchmark_function(array_resample_linear_benchmark,
  //                    "array_resample_linear_benchmark");
  // benchmark_function(array_resample_linear_old_benchmark,
  //                    "array_resample_linear_old_benchmark");
  // benchmark_function(array_resample_linear_benchmark,
  //                    "array_resample_linear2_benchmark");

  return 0;
}