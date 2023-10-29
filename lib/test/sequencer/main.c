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
#include "../../chain.h"
#include "../../sequencer.h"

int main() {
  Chain *chain = Chain_create();
  Chain_add(chain, 0, 0, 91);
  Chain_add(chain, 0, 1, 92);
  Chain_add(chain, 0, 2, 93);
  Chain_add(chain, 0, 3, 94);
  // this one will not be played, it will be the beginning of the next
  Chain_add(chain, 0, 4, 91 + 13);
  Sequencer_print(&chain->seq[0]);
  // Chain_quantize(chain, 0, 6);

  // Chain_add(chain, 1, 5, 2);
  // Chain_add(chain, 1, 6, 2 + 7);
  // Chain_add(chain, 1, 7, 2 + 13);
  // // this one will not be played, it will be the beginning of the next
  // Chain_add(chain, 1, 8, 2 + 21);
  // // Chain_quantize(chain, 1, 4);

  // uint8_t links[] = {0, 1, 1};
  // Chain_link(chain, links, 3);

  // printf("%d\n", round_uint16_to(0, 4));
  // printf("%d\n", round_uint16_to(1, 4));
  // printf("%d\n", round_uint16_to(7, 4));
  // printf("%d\n", round_uint16_to(13, 4));
  // // BEAT MUST START ON 1!
  // for (uint16_t i = 1; i < 60; i++) {
  //   int8_t beat = Chain_emit(chain, i);
  //   if (beat != -1) {
  //     printf("chain %d -> beat %d -> key %d\n", Chain_get_current(chain), i,
  //            beat);
  //   }
  // }

  return 0;
}