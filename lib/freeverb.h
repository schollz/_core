// Copyright 2023-2025 Zack Scholl, GPLv3.0

#include <stdio.h>
#include <stdlib.h>

#include "fixedpoint.h"

/* all pass filter */
typedef struct FV_AllPass {
  float feedback;
  int32_t *buffer;
  int bufsize;
  int bufidx;
} FV_AllPass;

void FV_AllPass_set_feedback(FV_AllPass *self, float feedback) {
  self->feedback = feedback;
}

FV_AllPass *FV_AllPass_malloc(int bufsize, float feedback) {
  FV_AllPass *self = (FV_AllPass *)malloc(sizeof(FV_AllPass));
  self->buffer = (int32_t *)malloc(bufsize * sizeof(int32_t));
  for (int i = 0; i < bufsize; i++) {
    self->buffer[i] = 0;
  }
  self->bufsize = bufsize;
  self->bufidx = 0;
  self->feedback = feedback;
  return self;
}

void FV_AllPass_free(FV_AllPass *self) {
  if (self == NULL) {
    return;
  }
  if (self->buffer) {
    free(self->buffer);
  }
  free(self);
}

int32_t FV_AllPass_process(FV_AllPass *self, int32_t input) {
  int32_t output;
  int32_t bufout;

  bufout = self->buffer[self->bufidx];

  output = -input + bufout;
  self->buffer[self->bufidx] = input + (bufout * self->feedback);

  if (++(self->bufidx) >= self->bufsize) self->bufidx = 0;

  return output;
}

/* all pass filter */
typedef struct FV_Comb {
  float feedback;
  float filterstore;
  float damp1;
  float damp2;
  int32_t *buffer;
  int bufsize;
  int bufidx;
} FV_Comb;

void FV_Comb_set_feedback(FV_Comb *self, float feedback) {
  self->feedback = feedback;
}

void FV_Comb_set_damp(FV_Comb *self, float damp) {
  self->damp1 = damp;
  self->damp2 = 1.0 - damp;
}

FV_Comb *FV_Comb_malloc(int bufsize, float feedback, float damp) {
  FV_Comb *self = (FV_Comb *)malloc(sizeof(FV_Comb));
  self->feedback = feedback;
  self->filterstore = 0;
  self->damp1 = damp;
  self->damp2 = 1.0 - damp;
  self->bufidx = 0;
  self->buffer = (int32_t *)malloc(bufsize * sizeof(int32_t));
  for (int i = 0; i < bufsize; i++) {
    self->buffer[i] = 0;
  }
  self->bufsize = bufsize;
  return self;
}

void FV_Comb_free(FV_Comb *self) {
  //   printf("[FV_Comb_free] freeing comb\n");
  if (self) {
    if (self->buffer) {
      //   printf("[FV_Comb_free] freeing buffer\n");
      free(self->buffer);
    }
    // printf("[FV_Comb_free] freeing self\n");
    free(self);
  }
}

static inline int32_t FV_Comb_process(FV_Comb *self, int32_t input) {
  int32_t output = self->buffer[self->bufidx];
  self->filterstore =
      (output * self->damp2) + (self->filterstore * self->damp1);
  self->buffer[self->bufidx] = input + (self->filterstore * self->feedback);
  if (++self->bufidx >= self->bufsize) self->bufidx = 0;
  return output;
}

// tuning
#define FV_NUMCOMBS_MAX 8
#define FV_NUMALLPASSES_MAX 4
#define FV_MUTED 0
#define FV_FIXEDGAIN ((0.015f))
#define FV_SCALEWET (3.0f)
#define FV_SCALEDRY (2.0f)
#define FV_SCALEDAMP ((0.4f))
#define FV_SCALEROOM ((0.28f))
#define FV_OFFSETROOM ((0.7f))
#define FV_INITIALROOM ((0.75f))
#define FV_INITIALDAMP ((1.0f))
#define FV_INITIALWET ((0.75f))
#define FV_INITIALDRY ((0.25f))
#define FV_INITIALWIDTH 1.0f
#define FV_INITIALMODE 0
#define FV_FREEZEMODE 0.5f
#define FV_STEREOSPREAD 23
const int combtunings[8] = {1116, 1557, 1188, 1491, 1356, 1277, 1422, 1617};
const int allpasstunings[4] = {556, 441, 341, 225};

typedef struct FV_Reverb {
  float gain;
  float roomsize;
  float damp;
  float wet, wet1, wet2;
  float dry;
  float width;
  int8_t num_combs;
  int8_t num_allpasses;

  // Comb filters
  FV_Comb *combL[FV_NUMCOMBS_MAX];
  FV_Comb *combR[FV_NUMCOMBS_MAX];

  // Allpass filters
  FV_AllPass *allpassL[FV_NUMALLPASSES_MAX];
  FV_AllPass *allpassR[FV_NUMALLPASSES_MAX];

} FV_Reverb;

int FV_Reverb_heap_size(int num_combs, int num_allpasses) {
  int total_size = sizeof(FV_Reverb);
  for (int i = 0; i < num_combs; i++) {
    total_size += 2 * sizeof(FV_Comb) + sizeof(int32_t) * combtunings[i] +
                  sizeof(int32_t) * (combtunings[i] + FV_STEREOSPREAD);
  }
  for (int i = 0; i < num_allpasses; i++) {
    total_size += 2 * sizeof(FV_AllPass) + sizeof(int32_t) * allpasstunings[i] +
                  sizeof(int32_t) * (allpasstunings[i] + FV_STEREOSPREAD);
  }

  return total_size;
}

void FV_Reverb_set_roomsize(FV_Reverb *self, float roomsize) {
  self->roomsize = (roomsize * FV_SCALEROOM) + FV_OFFSETROOM;
  for (int i = 0; i < self->num_combs; i++) {
    FV_Comb_set_feedback(self->combL[i], self->roomsize);
    FV_Comb_set_feedback(self->combR[i], self->roomsize);
  }
}

void FV_Reverb_set_damp(FV_Reverb *self, float damp) {
  self->damp = (damp * FV_SCALEDAMP);
  for (int i = 0; i < self->num_combs; i++) {
    FV_Comb_set_damp(self->combL[i], self->damp);
    FV_Comb_set_damp(self->combR[i], self->damp);
  }
  for (int i = 0; i < self->num_allpasses; i++) {
    FV_AllPass_set_feedback(self->allpassL[i], self->damp);
    FV_AllPass_set_feedback(self->allpassR[i], self->damp);
  }
}

void FV_Reverb_set_wet(FV_Reverb *self, float wet) {
  self->wet = (wet * FV_SCALEWET);
  self->dry = (1.0 - wet * FV_SCALEDRY);
  self->wet1 = self->wet * (self->width / 2 + 0.5);
  self->wet2 = self->wet * ((1 - self->width) / 2);
}

FV_Reverb *FV_Reverb_malloc(float roomsize, float damp, float wet, float dry) {
  int8_t num_allpasses = 3;
  int8_t num_combs = 8;
  for (int i = 0; i <= FV_NUMALLPASSES_MAX; i++) {
    num_combs = i;
    if (getFreeHeap() < FV_Reverb_heap_size(num_combs, num_allpasses)) {
      break;
    }
  }
  if (num_combs <= 0 || num_allpasses <= 0) {
    return NULL;
  }
  printf("[FV_Reverb_malloc] num_combs: %d, num_allpasses: %d\n", num_combs,
         num_allpasses);

  FV_Reverb *self = (FV_Reverb *)malloc(sizeof(FV_Reverb));
  if (self == NULL) {
    return NULL;
  }
  self->num_combs = num_combs;
  self->num_allpasses = num_allpasses;
  self->width = Q16_16_1;
  self->roomsize = (roomsize * FV_SCALEROOM) + FV_OFFSETROOM;
  self->damp = (damp * FV_SCALEDAMP);
  self->wet = (wet * FV_SCALEWET);
  self->dry = (dry * FV_SCALEDRY);
  self->wet1 = self->wet * (self->width / 2 + 0.5);
  self->wet2 = self->wet * ((1 - self->width) / 2);

  self->gain = (1.0 / (float)(self->num_combs + self->num_allpasses) / 6.0f);

  for (int i = 0; i < self->num_combs; i++) {
    self->combL[i] = FV_Comb_malloc(combtunings[i], self->roomsize, self->damp);
    if (self->combL[i] == NULL) {
      self->num_combs = i + 1;
      break;
    }
    self->combR[i] = FV_Comb_malloc(combtunings[i] + FV_STEREOSPREAD,
                                    self->roomsize, self->damp);
    if (self->combR[i] == NULL) {
      FV_Comb_free(self->combL[i]);
      self->num_combs = i + 1;
      break;
    }
  }
  for (int i = 0; i < self->num_allpasses; i++) {
    self->allpassL[i] = FV_AllPass_malloc(allpasstunings[i], self->damp);
    self->allpassR[i] =
        FV_AllPass_malloc(allpasstunings[i] + FV_STEREOSPREAD, self->damp);
  }
  // printf("[freeverb_fp] allocated\n");
  return self;
}

void FV_Reverb_free(FV_Reverb *self) {
  if (self == NULL) {
    return;
  }
  // printf("[freeverb_fp] freeing %d combs\n", self->num_combs);
  for (int i = 0; i < self->num_combs; i++) {
    // printf("[freeverb_fp] freeing combl %d\n", i);
    FV_Comb_free(self->combL[i]);
    // printf("[freeverb_fp] freeing combr %d\n", i);
    FV_Comb_free(self->combR[i]);
  }
  // printf("[freeverb_fp] freeing allpasses\n");
  for (int i = 0; i < self->num_allpasses; i++) {
    FV_AllPass_free(self->allpassL[i]);
    FV_AllPass_free(self->allpassR[i]);
  }
  // printf("[freeverb_fp] freeing self\n");
  free(self);
}

void FV_Reverb_process(FV_Reverb *self, int32_t *buf, unsigned int nr_samples) {
  int32_t outL, outR, inputL, inputR;
  for (int i = 0; i < nr_samples; i++) {
    outL = outR = 0;
    // convert int32_t to float
    inputL = (buf[2 * i + 0] * self->gain);
    inputR = (buf[2 * i + 1] * self->gain);

    // accumluate comb filters in parallel
    for (int j = 0; j < self->num_combs; j++) {
      outL += FV_Comb_process(self->combL[j], inputL);
      outR += FV_Comb_process(self->combR[j], inputR);
    }

    // feed through allpasses in series
    for (int j = 0; j < self->num_allpasses; j++) {
      outL = FV_AllPass_process(self->allpassL[j], outL);
      outR = FV_AllPass_process(self->allpassR[j], outR);
    }

    // calculate output mixing with anything already there
    buf[2 * i + 0] = q16_16_multiply(buf[2 * i + 0], self->dry) +
                     q16_16_multiply(outL, self->wet);
    buf[2 * i + 1] = q16_16_multiply(buf[2 * i + 1], self->dry) +
                     q16_16_multiply(outR, self->wet);
  }
}
