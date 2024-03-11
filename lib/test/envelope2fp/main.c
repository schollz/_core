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

//  gcc -o main main.c && ./main  | gnuplot -p -e 'plot "/dev/stdin"  using 0:1'
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../envelope2_fp.h"

int main() {
  // Create a Envelope2 instance
  float sampleRate = 441;

  Envelope2 *envelope2;

  // envelope2= Envelope2_create(sampleRate, 3, 20, 2);
  // for (int i = 0; i < 441 * 2; i++) {
  //   float value = Envelope2_update(envelope2);
  //   printf("%2.3f\n", value);
  // }
  // envelope2 = Envelope2_create(sampleRate, 20, 0.01, 1);
  // for (int i = 0; i < 441; i++) {
  //   float value = Envelope2_update(envelope2);
  //   printf("%2.3f\n", value);
  // }
  // Envelope2_destroy(envelope2);

  envelope2 = Envelope2_create(sampleRate, 1, -1, 1);
  for (int i = 0; i < 441; i++) {
    float value = Envelope2_update(envelope2);
    printf("%2.3f\n", value);
  }
  Envelope2_reset(envelope2, sampleRate, -1, 1, 1);
  for (int i = 0; i < 441; i++) {
    float value = Envelope2_update(envelope2);
    printf("%2.3f\n", value);
  }
  Envelope2_destroy(envelope2);

  return 0;
}