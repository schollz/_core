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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../envelope1.h"

int main() {
  // Create a Envelope1 instance
  uint32_t sampleRate = 44100;
  Envelope1 *envelope1 = Envelope1_create(sampleRate, 0, 8000, 44100);

  for (int i = 0; i < 44100; i++) {
    int32_t value = Envelope1_update(envelope1);
    printf("%d\n", value);
  }

  envelope1 = Envelope1_create(sampleRate, 0, 5241, 4000);
  for (int i = 0; i < 44100; i++) {
    int32_t value = Envelope1_update(envelope1);
    printf("%d\n", value);
  }

  envelope1 = Envelope1_create(sampleRate, 5241, 0, 4000);
  for (int i = 0; i < 44100; i++) {
    int32_t value = Envelope1_update(envelope1);
    printf("%d\n", value);
  }

  Envelope1_destroy(envelope1);

  return 0;
}