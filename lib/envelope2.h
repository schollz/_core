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

#ifndef ENVELOPE2_LIB

typedef struct Envelope2 {
  uint32_t mSampleRate;
  float start;
  float stop;
  float curr;
  float acc;
  uint32_t t;
  uint32_t duration_samples;
} Envelope2;

void Envelope2_reset(Envelope2 *envelope2, uint32_t mSampleRate, float start,
                     float stop, float duration_time) {
  envelope2->mSampleRate = mSampleRate;
  envelope2->start = start;
  envelope2->stop = stop;
  envelope2->curr = envelope2->start;
  envelope2->duration_samples = (uint32_t)round(mSampleRate * duration_time);
  envelope2->acc =
      (envelope2->stop - envelope2->start) / envelope2->duration_samples;
  envelope2->t = 0;
}

Envelope2 *Envelope2_create(uint32_t mSampleRate, float start, float stop,
                            float duration_time) {
  Envelope2 *envelope2 = (Envelope2 *)malloc(sizeof(Envelope2));
  Envelope2_reset(envelope2, mSampleRate, start, stop, duration_time);
  return envelope2;
}

float Envelope2_update(Envelope2 *envelope2) {
  if (envelope2->t < envelope2->duration_samples) {
    envelope2->t++;
    envelope2->curr = cos(M_PI * (envelope2->t) / envelope2->duration_samples);
    envelope2->curr = (-0.5 * envelope2->curr + 0.5);
    envelope2->curr = envelope2->curr * (envelope2->stop - envelope2->start) +
                      envelope2->start;
  }
  return (envelope2->curr);
  // return exp(envelope2->curr);
}

float Envelope2_get(Envelope2 *envelope2) { return (envelope2->curr); }

void Envelope2_destroy(Envelope2 *envelope2) { free(envelope2); }

#endif /* Envelope2_LIB */
#define ENVELOPE2_LIB 1