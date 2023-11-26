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

//  gcc -o main main.c && ./main  | gnuplot -p -e 'plot "/dev/stdin"  using 0:1
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../noise.h"
#include "../../resonantfilter.h"

int main() {
  // Seed for random number generation (you can change this to any other value)
  uint32_t seed = 1234;
  uint32_t sampleRate = 44100;
  // Create a Noise instance
  Noise *noise = Noise_create(seed, sampleRate);
  ResonantFilter *rf = ResonantFilter_create(0);
  ResonantFilter_setFc(rf, 45);
  ResonantFilter_setQ(rf, 0);

  // Generate and print some random noise using LFNoise0
  int32_t frequency = 200;  // Frequency in Hz
  uint8_t v = 0;
  for (int i = 0; i < sampleRate * 0.1; i++) {
    v++;
    // printf("%d\n", v);

    printf("%d\n", ResonantFilter_update(rf, v));
  }

  // Don't forget to free the memory when you're done using the Noise struct
  Noise_destroy(noise);

  return 0;
}