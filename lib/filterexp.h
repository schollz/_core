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

#ifndef FILTEREXP_LIB
#define FILTEREXP_LIB 1

#define ALPHA_MAX 512

typedef struct FilterExp {
  int alpha;
  int filtered;
} FilterExp;

FilterExp *FilterExp_create(int alpha) {
  FilterExp *fe = (FilterExp *)malloc(sizeof(FilterExp));
  fe->filtered = 0;
  fe->alpha = alpha;
  return fe;
}

void FilterExp_free(FilterExp *fe) { free(fe); }

int FilterExp_update(FilterExp *fe, int x) {
  fe->filtered = fe->alpha * x + (ALPHA_MAX - fe->alpha) * fe->filtered;
  fe->filtered = fe->filtered / ALPHA_MAX;
  return fe->filtered;
}

typedef struct FilterExpUint32 {
  uint32_t alpha;
  uint32_t filtered;
} FilterExpUint32;

FilterExpUint32 *FilterExpUint32_create(uint32_t alpha) {
  FilterExpUint32 *fe = (FilterExpUint32 *)malloc(sizeof(FilterExpUint32));
  fe->filtered = 0;
  fe->alpha = alpha;
  return fe;
}

void FilterExpUint32_free(FilterExpUint32 *fe) { free(fe); }

uint32_t FilterExpUint32_update(FilterExpUint32 *fe, uint32_t x) {
  fe->filtered = fe->alpha * x + (ALPHA_MAX - fe->alpha) * fe->filtered;
  fe->filtered = fe->filtered / ALPHA_MAX;
  return fe->filtered;
}

#endif