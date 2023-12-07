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

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include "../../fixedpoint.h"

int main() {
  int32_t a = q16_16_int32_to_fp(-80000);
  int32_t b = q16_16_float_to_fp(-0.25);
  printf("a * b = %d\n", q16_16_fp_to_int32(q16_16_multiply(a, b)));

  int32_t v = -75000;

  printf("%d\n", q16_16_multiply(v, q16_16_float_to_fp(0.5123)));

  int32_t start = 0;
  for (int32_t i = start; i < start + Q16_16_2PI; i += Q16_16_2PI / 96) {
    int32_t l = q16_16_multiply(v, q16_16_sin01(i));
    int32_t r = q16_16_multiply(v, Q16_16_1 - q16_16_sin01(i));
    printf("%d %d\n", l, r);
  }

  return 0;
}