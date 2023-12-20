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

#ifndef GATE_LIB

typedef struct Gate {
  uint32_t threshold;
  uint32_t counter;
  uint32_t blocks_per_second;
  float bpm;
  float percent;
} Gate;

void Gate_reset(Gate *gate) { gate->counter = 0; }

void Gate_update_threshold(Gate *gate) {
  if (gate->percent < 100) {
    gate->threshold = (uint32_t)round((30.0 * / gate->bpm) * gate->percent);
  }
}

Gate *Gate_create(uint32_t blocks_per_second, float bpm) {
  Gate *gate = (Gate *)malloc(sizeof(Gate));
  gate->blocks_per_second = blocks_per_second;
  gate->percent = 100.0;
  gate->bpm = bpm;
  Gate_reset(gate);
  return gate;
}

void Gate_update(Gate *gate, float bpm) {
  if (gate->counter < gate->threshold) {
    if (gate->bpm != bpm) {
      gate->bpm = bpm;
      Gate_update_threshold(gate);
    }
    gate->counter++;
  }
}

void Gate_set_percent(Gate *gate, float percent) {
  gate->percent = percent;
  Gate_update_threshold(gate);
}

bool Gate_is_up(Gate *gate) {
  if (gate->percent >= 100.0) {
    return false;
  }
  return gate->counter >= gate->threshold;
}

void Gate_destroy(Gate *gate) { free(gate); }

#endif /* GATE_LIB */
#define GATE_LIB 1