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

#ifndef ENVELOPE3_LIB

typedef struct Envelope3 {
  uint32_t mSampleRate;
  float start;
  float m1;
  float m2;
  float stop;
  float curr;
  uint8_t stage;
  uint32_t t;
  uint32_t duration_samples1;
  uint32_t duration_samples2;
  uint32_t duration_samples3;
} Envelope3;

Envelope3 *Envelope3_create(uint32_t mSampleRate, float start, float m1,
                            float m2, float stop, float duration_time1,
                            float duration_time2, float duration_time3) {
  Envelope3 *envelope3 = (Envelope3 *)malloc(sizeof(Envelope3));
  envelope3->mSampleRate = mSampleRate;
  envelope3->start = start;
  envelope3->m1 = m1;
  envelope3->m2 = m2;
  envelope3->stop = stop;
  envelope3->curr = envelope3->start;
  envelope3->duration_samples1 = (uint32_t)round(mSampleRate * duration_time1);
  envelope3->duration_samples2 = (uint32_t)round(mSampleRate * duration_time2);
  envelope3->duration_samples3 = (uint32_t)round(mSampleRate * duration_time3);
  envelope3->stage = 0;
  envelope3->t = 0;
  return envelope3;
}

float Envelope3_update(Envelope3 *envelope3) {
  if (envelope3->stage == 0 && envelope3->t < envelope3->duration_samples1) {
    envelope3->t++;
    envelope3->curr = (-0.5 * cos(6.283038530717958 * (envelope3->t) /
                                  envelope3->duration_samples1 / 2) +
                       0.5) *
                          (envelope3->m1 - envelope3->start) +
                      envelope3->start;
    if (envelope3->t == envelope3->duration_samples1) {
      envelope3->stage = 1;
      envelope3->t = 0;
    }
  } else if (envelope3->stage == 1 &&
             envelope3->t < envelope3->duration_samples2) {
    envelope3->t++;
    envelope3->curr = (-0.5 * cos(6.283038530717958 * (envelope3->t) /
                                  envelope3->duration_samples2 / 2) +
                       0.5) *
                          (envelope3->m2 - envelope3->m1) +
                      envelope3->m1;
    if (envelope3->t == envelope3->duration_samples2) {
      envelope3->stage = 2;
      envelope3->t = 0;
    }
  } else if (envelope3->stage == 2 &&
             envelope3->t < envelope3->duration_samples2) {
    envelope3->t++;
    envelope3->curr = (-0.5 * cos(6.283038530717958 * (envelope3->t) /
                                  envelope3->duration_samples3 / 2) +
                       0.5) *
                          (envelope3->stop - envelope3->m2) +
                      envelope3->m2;
    if (envelope3->t == envelope3->duration_samples3) {
      envelope3->stage = 3;
      envelope3->t = 0;
    }
  }
  return (envelope3->curr);
  // return exp(envelope3->curr);
}

void Envelope3_destroy(Envelope3 *envelope3) { free(envelope3); }

#endif /* Envelope3_LIB */
#define ENVELOPE3_LIB 1