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

#include "../../chain.h"
#include "../../sequencer.h"

int main() {
  Chain *chain = Chain_create();
  Chain_add(chain, 0, 0, 96);
  Chain_add(chain, 0, 1, 96 + 12);
  Chain_add(chain, 0, 2, 96 + 17);
  // this one will not be played, its meant to hold the end
  Chain_add(chain, 0, 3, 96 + 27);

  Chain_add(chain, 1, 5, 1);
  Chain_add(chain, 1, 6, 3);
  Chain_add(chain, 1, 7, 5);
  // this one will not be played, its meant to hold the end
  Chain_add(chain, 1, 8, 10);

  uint8_t links[] = {0, 1, 1};
  Chain_link(chain, links, 3);

  for (uint16_t i = 0; i < 60; i++) {
    int8_t beat = Chain_emit(chain, i);
    if (beat != -1) {
      printf("beat %d, chain %d: %d\n", i, Chain_get_current(chain), beat);
    }
  }

  return 0;
}