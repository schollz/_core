// Copyright 2023-2025 Zack Scholl, GPLv3.0

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

void EnvelopeGate_reset(EnvelopeGate *envelope, uint32_t mSampleRate,
                        float start, float stop, float duration_time1,
                        float duration_time2) {
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
}

EnvelopeGate *EnvelopeGate_create(uint32_t mSampleRate, float start, float stop,
                                  float duration_time1, float duration_time2) {
  EnvelopeGate *envelope = (EnvelopeGate *)malloc(sizeof(EnvelopeGate));
  EnvelopeGate_reset(envelope, mSampleRate, start, stop, duration_time1,
                     duration_time2);
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