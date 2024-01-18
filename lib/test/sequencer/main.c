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

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define NOSDCARD 1
#include "../../sequencer.h"

void sequencer_emit(uint8_t key) { printf("\nkey %d\n", key); }
void sequencer_stop() { printf("stop\n"); }

int main() {
  Sequencer *seq = Sequencer_malloc();
  Sequencer_set_callbacks(seq, sequencer_emit, sequencer_stop);
  Sequencer_add(seq, 1, 0);
  Sequencer_add(seq, 2, 3);
  Sequencer_add(seq, 3, 6);
  Sequencer_add(seq, 4, 9);
  Sequencer_add(seq, 5, 12);

  Sequencer_play(seq, false);
  for (int i = 0; i < 21; i++) {
    printf("[%d] ", i);
    Sequencer_step(seq, i);
    if (!Sequencer_is_playing(seq)) {
      Sequencer_continue(seq);
    }
  }
  printf("\n");

  Sequencer_quantize(seq, 4);
  Sequencer_play(seq, true);
  for (int i = 0; i < 30; i++) {
    printf("[%d] ", i);
    Sequencer_step(seq, i);
  }

  Sequencer_free(seq);
  return 0;
}