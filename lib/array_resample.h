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

#include "fixedpoint.h"
#define INTERPOLATE_VALUE 512

int16_t *array_resample_linear2(int16_t *arr, int16_t arr_size,
                                int16_t newSize) {
  if (arr_size <= 0 || newSize <= 0) {
    return NULL;  // Invalid input
  }

  int16_t *newArr = (int16_t *)malloc(sizeof(int16_t) * newSize);
  if (newArr == NULL) {
    return NULL;  // Memory allocation error
  }

  float step = (float)(arr_size - 1) / (newSize - 1);

  for (int16_t i = 0; i < newSize; ++i) {
    float index = i * step;
    int16_t lowerIdx = (int16_t)index;
    int16_t upperIdx = lowerIdx + 1;

    if (upperIdx >= arr_size) {
      newArr[i] = arr[lowerIdx];
    } else {
      float fraction = index - lowerIdx;
      newArr[i] =
          (int16_t)((1 - fraction) * arr[lowerIdx] + fraction * arr[upperIdx]);
    }
  }

  return newArr;
}

int16_t *array_resample_quadratic(int16_t *arr, int16_t arr_size,
                                  int16_t newSize) {
  if (arr_size <= 0 || newSize <= 0) {
    return NULL;  // Invalid input
  }

  int16_t *newArr = (int16_t *)malloc(sizeof(int16_t) * newSize);
  if (newArr == NULL) {
    return NULL;  // Memory allocation error
  }

  float step = (float)(arr_size - 1) / (newSize - 1);

  for (int16_t i = 0; i < newSize; ++i) {
    float index = i * step;
    int16_t baseIdx = (int16_t)index;
    int16_t lowerIdx = baseIdx - 1;
    int16_t upperIdx = baseIdx + 1;

    if (lowerIdx < 0) {
      lowerIdx = 0;
    }
    if (upperIdx >= arr_size) {
      upperIdx = arr_size - 1;
    }

    float fraction = index - baseIdx;

    int16_t x0 = arr[lowerIdx];
    int16_t x1 = arr[baseIdx];
    int16_t x2 = arr[upperIdx];

    float result = (x0 - 2 * x1 + x2);
    result = x2 - x0 + fraction * result;
    result = fraction * result;
    result = x1 + 0.5 * result;
    newArr[i] = (int16_t)(result);
  }

  return newArr;
}

int16_t *array_resample_quadratic_fp(int16_t *arr, int16_t arr_size,
                                     int16_t newSize) {
  int32_t *arrFP = (int32_t *)malloc(sizeof(int32_t) * arr_size);
  int16_t *newArr = (int16_t *)malloc(sizeof(int16_t) * newSize);
  for (uint16_t i = 0; i < arr_size; i++) {
    arrFP[i] = q16_16_int16_to_fp(arr[i]);
  }

  int32_t step = q16_16_float_to_fp((float)(arr_size - 1) / (newSize - 1));
  int32_t index = Q16_16_0;

  for (int16_t i = 0; i < newSize; ++i) {
    int16_t baseIdx = q16_16_fp_to_int16((index >> 16) << 16);
    int16_t lowerIdx = baseIdx - 1;
    int16_t upperIdx = baseIdx + 1;

    if (lowerIdx < 0) {
      lowerIdx = 0;
    }
    if (upperIdx >= arr_size) {
      upperIdx = arr_size - 1;
    }

    int32_t fraction = index - ((index >> 16) << 16);

    int32_t result = arrFP[lowerIdx] -
                     q16_16_multiply(Q16_16_2, arrFP[baseIdx]) +
                     arrFP[upperIdx];
    result =
        arrFP[upperIdx] - arrFP[lowerIdx] + q16_16_multiply(fraction, result);
    result = q16_16_multiply(fraction, result);
    result = arrFP[baseIdx] + q16_16_multiply(Q16_16_0_5, result);
    newArr[i] = q16_16_fp_to_int16(result);
    index += step;
  }

  free(arrFP);

  return newArr;
}

int16_t *array_resample_linear_old(int16_t *arr, int16_t arr_size,
                                   int16_t newSize) {
  int16_t *newArray;
  newArray = malloc(sizeof(int16_t) * newSize);
  if (arr_size == newSize) {
    for (int16_t i = 0; i < newSize; i++) {
      newArray[i] = arr[i];
    }
    return newArray;
  }

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

int16_t *__not_in_flash_func(array_resample_linear)(int16_t *arr,
                                                    int16_t arr_size,
                                                    int16_t newSize) {
  // Allocate memory for the new array
  int16_t *newArray = malloc(sizeof(int16_t) * newSize);
  if (!newArray) {
    // Handle memory allocation failure
    return NULL;
  }

  // If the sizes match, simply copy the input array
  if (arr_size == newSize) {
    for (int16_t i = 0; i < newSize; i++) {
      newArray[i] = arr[i];
    }
    return newArray;
  }

  // Calculate step size in fixed-point format
  uint32_t stepSize = (arr_size)*INTERPOLATE_VALUE / (newSize);

  for (int16_t i = 0; i < newSize; i++) {
    uint32_t indexFixed = i * stepSize;               // Fixed-point index
    uint32_t index = indexFixed / INTERPOLATE_VALUE;  // Integer part
    uint32_t frac = indexFixed % INTERPOLATE_VALUE;   // Fractional part

    // Perform fixed-point linear interpolation
    int32_t x = ((int32_t)arr[index] * (INTERPOLATE_VALUE - frac)) +
                ((int32_t)arr[index + 1] * frac);
    newArray[i] = x / INTERPOLATE_VALUE;
  }

  return newArray;
}

int16_t *buffer_input(int16_t *arr, int16_t arr_size) {
  if (arr_size < 2) {
    return NULL;  // Handle invalid input
  }

  int16_t *buffered = (int16_t *)malloc((arr_size + 2) * sizeof(int16_t));
  if (buffered == NULL) {
    return NULL;  // Memory allocation failed
  }

  // Copy the original values to the buffered array
  for (int i = 0; i < arr_size; i++) {
    buffered[i + 1] = arr[i];
  }

  // Pad the end with two repeated values
  buffered[0] = arr[0];
  buffered[arr_size + 1] = arr[arr_size - 1];

  return buffered;
}

// Hermite interpolation function
int16_t *hermite_interpolation(int16_t *arr, int16_t arr_size,
                               int16_t newSize) {
  if (arr_size < 4 || newSize <= 0) {
    return NULL;  // Handle invalid input
  }

  int16_t *buffered = buffer_input(arr, arr_size);
  if (buffered == NULL) {
    return NULL;  // Memory allocation failed
  }

  int16_t *interpolated = (int16_t *)malloc(newSize * sizeof(int16_t));
  if (interpolated == NULL) {
    free(buffered);
    return NULL;  // Memory allocation failed
  }

  for (int i = 0; i < newSize; i++) {
    double t = (double)i / (newSize - 1);
    double s = t * (arr_size - 1);
    int16_t index = (int16_t)s;
    s -= index;

    // Hermite basis functions
    double h1 = 2 * s * s * s - 3 * s * s + 1;
    double h2 = -2 * s * s * s + 3 * s * s;
    double h3 = s * s * s - 2 * s * s + s;
    double h4 = s * s * s - s * s;

    // Perform Hermite interpolation between the values after initial padding
    // and before the last padding
    int16_t p0 = buffered[index + 1];
    int16_t p1 = buffered[index + 2];
    int16_t m0 = (buffered[index + 3] - buffered[index]) / 2;
    int16_t m1 = (buffered[index + 4] - buffered[index + 1]) / 2;

    interpolated[i] = (int16_t)(h1 * p0 + h2 * p1 + h3 * m0 + h4 * m1);
  }

  free(buffered);
  return interpolated;
}
