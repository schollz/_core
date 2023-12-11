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

#ifndef ENVELOPE_LINEAR_INTEGER_LIB

#include "utils.h"

typedef struct EnvelopeLinearInteger {
  int32_t curr;
  int32_t stop;
  bool forward;
  uint32_t t;
  uint32_t inc_t;
} EnvelopeLinearInteger;

EnvelopeLinearInteger *EnvelopeLinearInteger_reset(EnvelopeLinearInteger *env,
                                                   uint32_t mSampleRate,
                                                   int32_t start, int32_t stop,
                                                   float duration_time) {
  env->curr = start;
  env->stop = stop;
  env->forward = stop >= start;
  // increments +1 every t until reaching stop
  env->t = 0;
  env->inc_t = round(mSampleRate * duration_time / ((float)abs(stop - start)));
  // printf("[EnvelopeLinearInteger_reset] inc_t: %d\n", env->inc_t);
  // printf("[EnvelopeLinearInteger_reset] start: %d\n", start);
  // printf("[EnvelopeLinearInteger_reset] stop: %d\n", stop);
  // printf("[EnvelopeLinearInteger_update] env->forward: %d\n", env->forward);
  // printf("[EnvelopeLinearInteger_update] env->curr: %d\n", env->curr);
  // printf("[EnvelopeLinearInteger_update] env->stop: %d\n", env->stop);
  return env;
}

EnvelopeLinearInteger *EnvelopeLinearInteger_create(uint32_t mSampleRate,
                                                    int32_t start, int32_t stop,
                                                    float duration_time) {
  EnvelopeLinearInteger *env =
      (EnvelopeLinearInteger *)malloc(sizeof(EnvelopeLinearInteger));
  EnvelopeLinearInteger_reset(env, mSampleRate, start, stop, duration_time);
  return env;
}

int32_t EnvelopeLinearInteger_update(EnvelopeLinearInteger *env,
                                     callback_int32 changed) {
  if ((env->forward && env->curr >= env->stop) ||
      (!env->forward && env->curr <= env->stop)) {
    return env->stop;
  }
  env->t++;
  if (env->t >= env->inc_t) {
    env->t = 0;
    if (env->forward) {
      env->curr++;
    } else {
      env->curr--;
    }
    if (changed != NULL) {
      changed(env->curr);
    }
  }
  return env->curr;
}

void EnvelopeLinearInteger_destroy(EnvelopeLinearInteger *env) { free(env); }

#endif /* EnvelopeLinearInteger_LIB */
#define ENVELOPE_LINEAR_INTEGER_LIB 1