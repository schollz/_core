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

int main() {
  int16_t arr[] = {11620, 10309, 6301, 2171, 650, 2136, 4150, 4507};
  int16_t arr_size = sizeof(arr) / sizeof(arr[0]);
  int16_t arr_new_size = 80;
  int16_t *newArray = array_resample_linear(arr, arr_size, arr_new_size);
  for (int i = 0; i < arr_new_size; i++) {
    printf("%f,%d\n", (float)i / (float)(arr_new_size - 1), newArray[i]);
  }
  for (int i = 0; i < arr_size; i++) {
    printf("%f,%d\n", (float)i / (float)(arr_size - 1), arr[i]);
  }
  newArray = array_resample_linear2(arr, arr_size, arr_new_size);
  for (int i = 0; i < arr_new_size; i++) {
    printf("%f,%d\n", (float)i / (float)(arr_new_size - 1), newArray[i]);
  }
  newArray = array_resample_quadratic(arr, arr_size, arr_new_size);
  for (int i = 0; i < arr_new_size; i++) {
    printf("%f,%d\n", (float)i / (float)(arr_new_size - 1), newArray[i]);
  }

  free(newArray);

  return 0;
}