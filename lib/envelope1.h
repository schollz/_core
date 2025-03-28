// Copyright 2023-2025 Zack Scholl, GPLv3.0

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