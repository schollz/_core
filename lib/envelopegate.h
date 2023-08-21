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

#ifndef ENVELOPEGATE_LIB

typedef struct EnvelopeGate {
  uint32_t mSampleRate;
  float start;
  float stop;
  float curr;
  float acc;
  uint32_t t;
  uint8_t stage;
  uint32_t duration_samples1;
  uint32_t duration_samples2;
} EnvelopeGate;

EnvelopeGate *EnvelopeGate_create(uint32_t mSampleRate, float start, float stop,
                                  float duration_time1, float duration_time2) {
  EnvelopeGate *envelope = (EnvelopeGate *)malloc(sizeof(EnvelopeGate));
  envelope->mSampleRate = mSampleRate;
  envelope->start = (start);
  envelope->stop = (stop);
  envelope->curr = envelope->start;
  envelope->stage = 0;
  envelope->duration_samples1 = (uint32_t)round(mSampleRate * duration_time1);
  envelope->duration_samples2 = (uint32_t)round(mSampleRate * duration_time2);
  envelope->acc =
      (envelope->stop - envelope->start) / envelope->duration_samples2;
  envelope->t = 0;
  return envelope;
}

float EnvelopeGate_update(EnvelopeGate *envelope) {
  if (envelope->stage == 0 && envelope->t < envelope->duration_samples1) {
    envelope->t++;
    envelope->curr = envelope->start;
    if (envelope->t == envelope->duration_samples1) {
      envelope->stage = 1;
      envelope->t = 0;
    }
  } else if (envelope->stage == 1 &&
             envelope->t < envelope->duration_samples2) {
    envelope->t++;
    envelope->curr = (-0.5 * cos(6.283038530717958 * (envelope->t) /
                                 envelope->duration_samples2 / 2) +
                      0.5) *
                         (envelope->stop - envelope->start) +
                     envelope->start;
  }
  return (envelope->curr);
}

void EnvelopeGate_destroy(EnvelopeGate *envelope) { free(envelope); }

#endif /* EnvelopeGate_LIB */
#define ENVELOPEGATE_LIB 1