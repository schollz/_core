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

#include "../../comb.h"
#define NUM_SAMPLES 3000

int main() {
  int32_t samples[NUM_SAMPLES];
  for (uint32_t i = 0; i < NUM_SAMPLES / 2; i++) {
    samples[2 * i + 0] =
        (int32_t)(32767.0 * sin(2.0 * M_PI * 2000.0 * i / 44100.0));
    samples[2 * i + 1] =
        (int32_t)(32767.0 * sin(2.0 * M_PI * 2000.0 * i / 44100.0));
  }

  Comb *comb;
  comb = Comb_malloc();
  Comb_setActive(comb, true);
  Comb_process(comb, samples, NUM_SAMPLES / 2, 0);

  for (uint32_t i = 0; i < NUM_SAMPLES / 2; i++) {
    printf("%d,%d\n", i, samples[2 * i]);
  }

  Comb_free(comb);
  return 0;
}