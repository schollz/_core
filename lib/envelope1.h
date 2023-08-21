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

#ifndef ENVELOPE1_LIB

typedef struct Envelope1 {
  uint32_t mSampleRate;
  int32_t start;
  int32_t stop;
  int32_t curr;
  int32_t acc;
  int32_t m;
  int32_t t;
  uint32_t duration_samples;
} Envelope1;

Envelope1 *Envelope1_create(uint32_t mSampleRate, int32_t start, int32_t stop,
                            uint32_t duration_samples) {
  Envelope1 *envelope1 = (Envelope1 *)malloc(sizeof(Envelope1));
  envelope1->mSampleRate = mSampleRate;
  envelope1->start = start;
  envelope1->stop = stop;
  envelope1->curr = start;
  envelope1->t = 0;
  envelope1->m = 0;
  envelope1->acc = (stop - start) / duration_samples;
  if (envelope1->acc == 0 && (stop - start) > 0) {
    envelope1->m = duration_samples / (stop - start);
  }
  if (envelope1->acc == 0 && (start - stop) > 0) {
    envelope1->m = duration_samples / (start - stop);
  }
  envelope1->duration_samples = duration_samples;
  return envelope1;
}

int32_t Envelope1_update(Envelope1 *envelope1) {
  if (envelope1->t < envelope1->duration_samples &&
      envelope1->curr < envelope1->stop) {
    envelope1->t++;
    if (envelope1->acc > 0) {
      envelope1->curr = envelope1->curr + envelope1->acc;
    } else if (envelope1->t % envelope1->m == 0) {
      if (envelope1->start < envelope1->stop) {
        envelope1->curr++;
      } else {
        envelope1->curr--;
      }
    }
  }
  return envelope1->curr;
}

void Envelope1_destroy(Envelope1 *envelope1) { free(envelope1); }

#endif /* Envelope1_LIB */
#define ENVELOPE1_LIB 1