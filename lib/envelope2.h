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

Envelope2 *Envelope2_create(uint32_t mSampleRate, float start, float stop,
                            uint32_t duration_samples) {
  Envelope2 *envelope2 = (Envelope2 *)malloc(sizeof(Envelope2));
  envelope2->mSampleRate = mSampleRate;
  envelope2->start = (start);
  envelope2->stop = (stop);
  //   envelope2->start = log(start);
  //   envelope2->stop = log(stop);
  envelope2->curr = envelope2->start;
  envelope2->acc = (envelope2->stop - envelope2->start) / duration_samples;
  envelope2->t = 0;
  envelope2->duration_samples = duration_samples;
  return envelope2;
}

float Envelope2_update(Envelope2 *envelope2) {
  if (envelope2->t < envelope2->duration_samples) {
    envelope2->t++;
    envelope2->curr = (-0.5 * cos(6.283038530717958 * (envelope2->t) /
                                  envelope2->duration_samples / 2) +
                       0.5) *
                          (envelope2->stop - envelope2->start) +
                      envelope2->start;
  }
  return (envelope2->curr);
  // return exp(envelope2->curr);
}

void Envelope2_destroy(Envelope2 *envelope2) { free(envelope2); }

#endif /* Envelope2_LIB */
#define ENVELOPE2_LIB 1