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
