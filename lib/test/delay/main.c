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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../delay.h"
#include "../../sinewave.h"

int main() {
  init_sinewaves();
  SinOsc *osc1 = SinOsc_malloc();
  SinOsc *osc2 = SinOsc_malloc();
  SinOsc_wave(osc1, 24);
  SinOsc_quiet(osc1, 0);
  SinOsc_wave(osc2, 29);
  SinOsc_quiet(osc2, 1);
  Delay *dly = Delay_malloc();
  Delay_setDuration(dly, 1000);
  Delay_setActive(dly, true);
  int32_t vals[8000];
  for (int i = 0; i < 8000; i++) {
    if (i < 500) {
      vals[i] = (SinOsc_next(osc1) >> 17) + (SinOsc_next(osc2) >> 17);
    } else {
      vals[i] = 0;
    }
  }
  Delay_process(dly, vals, 8000);
  for (int i = 0; i < 8000; i++) {
    printf("%d\n", vals[i]);
  }

  SinOsc_free(osc1);
  SinOsc_free(osc2);
  Delay_free(dly);
  return 0;
}