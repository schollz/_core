// Copyright 2023-2025 Zack Scholl, GPLv3.0

#ifndef ENVELOPE2_FP_LIB
#define ENVELOPE2_FP_LIB 1
#include "fixedpoint.h"

typedef struct Envelope2 {
  int32_t mSampleRate;
  int32_t start;
  int32_t stop;
  int32_t curr;
  int32_t acc;
  int32_t t;
  int32_t duration_samples;
  float stop_float;
} Envelope2;

void Envelope2_reset(Envelope2 *envelope2, float mSampleRate, float start,
                     float stop, float duration_time) {
  envelope2->mSampleRate = q16_16_float_to_fp(mSampleRate);
  envelope2->start = q16_16_float_to_fp(start);
  envelope2->stop = q16_16_float_to_fp(stop);
  envelope2->stop_float = stop;
  envelope2->curr = envelope2->start;
  envelope2->duration_samples =
      q16_16_float_to_fp(round(mSampleRate * duration_time));
  envelope2->acc = q16_16_divide(envelope2->stop - envelope2->start,
                                 envelope2->duration_samples);
  envelope2->t = 0;
}

Envelope2 *Envelope2_create(float mSampleRate, float start, float stop,
                            float duration_time) {
  Envelope2 *envelope2 = (Envelope2 *)malloc(sizeof(Envelope2));
  Envelope2_reset(envelope2, mSampleRate, start, stop, duration_time);
  return envelope2;
}

float Envelope2_update(Envelope2 *envelope2) {
  if (envelope2->t < envelope2->duration_samples) {
    envelope2->t += Q16_16_1;
    envelope2->curr = q16_16_cos(q16_16_divide(
        q16_16_multiply(Q16_16_PI, envelope2->t), envelope2->duration_samples));
    envelope2->curr =
        (q16_16_multiply(-Q16_16_0_5, envelope2->curr) + Q16_16_0_5);
    envelope2->curr =
        q16_16_multiply(envelope2->curr, (envelope2->stop - envelope2->start)) +
        envelope2->start;
    return q16_16_fp_to_float(envelope2->curr);
  }
  return envelope2->stop_float;
}

bool Envelope2_is_active(Envelope2 *envelope2) {
  return envelope2->t < envelope2->duration_samples;
}

float Envelope2_get(Envelope2 *envelope2) {
  return q16_16_fp_to_float(envelope2->curr);
}

void Envelope2_destroy(Envelope2 *envelope2) { free(envelope2); }

#endif