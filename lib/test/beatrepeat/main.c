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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../beatrepeat.h"
#include "../../sinewave.h"

int main() {
  init_sinewaves();
  SinOsc *osc1 = SinOsc_malloc();
  SinOsc *osc2 = SinOsc_malloc();
  SinOsc_wave(osc1, 24);
  SinOsc_quiet(osc1, 0);
  SinOsc_wave(osc2, 29);
  SinOsc_quiet(osc2, 1);
  BeatRepeat *br = BeatRepeat_malloc();
  for (int i = 0; i < 1000; i++) {
    int16_t val = (SinOsc_next(osc1) >> 17) + (SinOsc_next(osc2) >> 17);
    BeatRepeat_process(br, val);
    printf("%d\n", val);
  }
  BeatRepeat_repeat(br, 200);
  for (int i = 0; i < 1000; i++) {
    int16_t val = (SinOsc_next(osc1) >> 17) + (SinOsc_next(osc2) >> 17);
    val = BeatRepeat_process(br, val);
    printf("%d\n", val);
  }
  BeatRepeat_repeat(br, 0);
  for (int i = 0; i < 1200; i++) {
    int16_t val = (SinOsc_next(osc1) >> 17) + (SinOsc_next(osc2) >> 17);
    val = BeatRepeat_process(br, val);
    printf("%d\n", val);
  }

  SinOsc_free(osc1);
  SinOsc_free(osc2);
  BeatRepeat_free(br);
  return 0;
}