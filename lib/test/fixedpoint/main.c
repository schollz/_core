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

  int32_t cons = q16_16_float_to_fp(1.25);
  int32_t cons2 = q16_16_float_to_fp(1.33);
  float i = 2.5;
  printf("q16_16_multiply(q16_16_int32_to_fp(i), cons)=%d\n",
         q16_16_multiply(q16_16_float_to_fp(i), cons));
  printf("q16_16_multiply(q16_16_int32_to_fp(i), cons2)=%d\n",
         q16_16_multiply(q16_16_float_to_fp(i), cons2));
  printf("%d\n",
         q16_16_multiply(q16_16_multiply(q16_16_float_to_fp(i), cons),
                         q16_16_multiply(q16_16_float_to_fp(i), cons2)));
  float f = q16_16_fp_to_float(
      q16_16_multiply(q16_16_multiply(q16_16_float_to_fp(i), cons),
                      q16_16_multiply(q16_16_float_to_fp(i), cons2)));
  printf("f=%2.1f\n", f);

  float bpm = 156.0;
  float bpm_source = 138.0;
  float pitch_change = 0.52973;
  uint32_t samples = 1000;

  printf("%2.3f\n", samples * bpm / bpm_source * pitch_change);
  printf("%2.3f\n",
         q16_16_fp_to_int32(
             samples *
             q16_16_multiply(q16_16_divide(q16_16_float_to_fp(bpm),
                                           q16_16_float_to_fp(bpm_source)),
                             q16_16_float_to_fp(pitch_change))));

  // for (float i = -3.14159 * 20; i < 32; i += 0.001) {
  //   printf("%2.3f\n", q16_16_fp_to_float(q16_16_cos(q16_16_float_to_fp(i))));
  // }
  return 0;
}