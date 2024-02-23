#include <stdio.h>

#include "fixedpoint.h"

typedef struct FV_AllPass {
  int32_t feedback;
  int32_t *buffer;
  int bufsize;
  int bufidx;
} FV_AllPass;

// Function implementations
void FV_AllPass_init(FV_AllPass *self) {
  self->buffer = NULL;
  self->feedback = Q16_16_0_5;
  self->bufsize = 0;
  self->bufidx = 0;
}

void FV_AllPass_setbuffer(FV_AllPass *self, int32_t *buf, int size) {
  if (self->buffer) {
    free(self->buffer);
  }
  self->buffer = buf;
  self->bufsize = size;
}

int32_t FV_AllPass_process(FV_AllPass *self, int32_t input) {
  int32_t output;
  int32_t bufout;

  bufout = self->buffer[self->bufidx];

  output = -input + bufout;
  self->buffer[self->bufidx] = input + q16_16_multiply(bufout, self->feedback);

  if (++(self->bufidx) >= self->bufsize) self->bufidx = 0;

  return output;
}

void FV_AllPass_mute(FV_AllPass *self) {
  for (int i = 0; i < self->bufsize; i++) self->buffer[i] = 0;
}

void FV_AllPass_setfeedback(FV_AllPass *self, int32_t val) {
  self->feedback = val;
}

// comb filter

typedef struct FV_Comb {
  int32_t feedback;
  int32_t filterstore;
  int32_t damp1;
  int32_t damp2;
  int32_t *buffer;
  int bufsize;
  int bufidx;
} FV_Comb;

void FV_Comb_init(FV_Comb *self) {
  self->feedback = Q16_16_0_5;
  self->filterstore = 0;
  self->damp1 = 0;
  self->damp2 = 0;
  self->buffer = NULL;
  self->bufsize = 0;
  self->bufidx = 0;
}

void FV_Comb_free(FV_Comb *self) {
  if (self->buffer) {
    free(self->buffer);
  }
  free(self);
}

static inline int32_t FV_Comb_process(FV_Comb *self, int32_t input) {
  int32_t output = self->buffer[self->bufidx];
  self->filterstore = q16_16_multiply(output, self->damp2) +
                      q16_16_multiply(self->filterstore, self->damp1);
  self->buffer[self->bufidx] =
      input + q16_16_multiply(self->filterstore, self->feedback);
  if (++self->bufidx >= self->bufsize) self->bufidx = 0;
  return output;
}

void FV_Comb_mute(FV_Comb *self) {
  for (int i = 0; i < self->bufsize; i++) self->buffer[i] = 0;
}

void FV_Comb_setbuffer(FV_Comb *self, int32_t *buf, int size) {
  if (self->buffer) {
    free(self->buffer);
  }
  self->buffer = buf;
  self->bufsize = size;
}

void FV_Comb_setfeedback(FV_Comb *self, int32_t val) { self->feedback = val; }

void FV_Comb_setdamp(FV_Comb *self, int32_t val) {
  self->damp1 = val;
  self->damp2 = Q16_16_1 - val;
}

// tuning
#define FV_NUMCOMBS 3
#define FV_NUMALLPASSES 3
#define FV_MUTED 0
#define FV_FIXEDGAIN (q16_16_float_to_fp(0.015f))
#define FV_SCALEWET (3 * Q16_16_1)
#define FV_SCALEDRY (2 * Q16_16_1)
#define FV_SCALEDAMP (q16_16_float_to_fp(0.4f))
#define FV_SCALEROOM (q16_16_float_to_fp(0.28f))
#define FV_OFFSETROOM (q16_16_float_to_fp(0.7f))
#define FV_INITIALROOM (q16_16_float_to_fp(0.8f))
#define FV_INITIALDAMP (q16_16_float_to_fp(0.15f))
#define FV_INITIALWET (q16_16_float_to_fp(0.7f))
#define FV_INITIALDRY (q16_16_float_to_fp(0.3f))
#define FV_INITIALWIDTH Q16_16_1
#define FV_INITIALMODE 0
#define FV_FREEZEMODE 0.5f
#define FV_STEREOSPREAD 23
#define FV_COMBTUNINGL1 1116
#define FV_COMBTUNINGR1 (1116 + FV_STEREOSPREAD)
#define FV_COMBTUNINGL2 1188
#define FV_COMBTUNINGR2 (1188 + FV_STEREOSPREAD)
#define FV_COMBTUNINGL3 1277
#define FV_COMBTUNINGR3 (1277 + FV_STEREOSPREAD)
#define FV_COMBTUNINGL4 1356
#define FV_COMBTUNINGR4 (1356 + FV_STEREOSPREAD)
#define FV_COMBTUNINGL5 1422
#define FV_COMBTUNINGR5 (1422 + FV_STEREOSPREAD)
#define FV_COMBTUNINGL6 1491
#define FV_COMBTUNINGR6 (1491 + FV_STEREOSPREAD)
#define FV_COMBTUNINGL7 1557
#define FV_COMBTUNINGR7 (1557 + FV_STEREOSPREAD)
#define FV_COMBTUNINGL8 1617
#define FV_COMBTUNINGR8 (1617 + FV_STEREOSPREAD)
#define FV_ALLPASSTUNINGL1 556
#define FV_ALLPASSTUNINGR1 (556 + FV_STEREOSPREAD)
#define FV_ALLPASSTUNINGL2 441
#define FV_ALLPASSTUNINGR2 (441 + FV_STEREOSPREAD)
#define FV_ALLPASSTUNINGL3 341
#define FV_ALLPASSTUNINGR3 (341 + FV_STEREOSPREAD)
#define FV_ALLPASSTUNINGL4 225
#define FV_ALLPASSTUNINGR4 (225 + FV_STEREOSPREAD)

typedef struct FV_Reverb {
  int32_t gain;
  int32_t roomsize, roomsize1;
  int32_t damp, damp1;
  int32_t wet, wet1, wet2;
  int32_t dry;
  int32_t width;
  int32_t mode;

  // Comb filters
  FV_Comb combL[FV_NUMCOMBS];
  FV_Comb combR[FV_NUMCOMBS];

  // Allpass filters
  FV_AllPass allpassL[FV_NUMALLPASSES];
  FV_AllPass allpassR[FV_NUMALLPASSES];

  // Buffers for the combs
  int32_t bufcombL1[FV_COMBTUNINGL1], bufcombR1[FV_COMBTUNINGR1];
  int32_t bufcombL2[FV_COMBTUNINGL2], bufcombR2[FV_COMBTUNINGR2];
  int32_t bufcombL3[FV_COMBTUNINGL3], bufcombR3[FV_COMBTUNINGR3];
  //   int32_t bufcombL4[FV_COMBTUNINGL4], bufcombR4[FV_COMBTUNINGR4];
  //   int32_t bufcombL5[FV_COMBTUNINGL5], bufcombR5[FV_COMBTUNINGR5];
  //   int32_t bufcombL6[FV_COMBTUNINGL6], bufcombR6[FV_COMBTUNINGR6];
  //   int32_t bufcombL7[FV_COMBTUNINGL7], bufcombR7[FV_COMBTUNINGR7];
  //   int32_t bufcombL8[FV_COMBTUNINGL8], bufcombR8[FV_COMBTUNINGR8];

  // Buffers for the allpasses
  int32_t bufallpassL1[FV_ALLPASSTUNINGL1], bufallpassR1[FV_ALLPASSTUNINGR1];
  int32_t bufallpassL2[FV_ALLPASSTUNINGL2], bufallpassR2[FV_ALLPASSTUNINGR2];
  int32_t bufallpassL3[FV_ALLPASSTUNINGL3], bufallpassR3[FV_ALLPASSTUNINGR3];
  //   int32_t bufallpassL4[FV_ALLPASSTUNINGL4],
  //   bufallpassR4[FV_ALLPASSTUNINGR4];
} FV_Reverb;

void FV_Reverb_mute(FV_Reverb *self) {
  for (int i = 0; i < FV_NUMCOMBS; i++) {
    FV_Comb_mute(&self->combL[i]);
    FV_Comb_mute(&self->combR[i]);
  }
  for (int i = 0; i < FV_NUMALLPASSES; i++) {
    FV_AllPass_mute(&self->allpassL[i]);
    FV_AllPass_mute(&self->allpassR[i]);
  }
}

void FV_Reverb_update(FV_Reverb *self) {
  int i;

  self->wet1 = self->wet * (self->width / 2 + 0.5);
  self->wet2 = self->wet * ((1 - self->width) / 2);

  //   if (self->mode >= FV_FREEZEMODE) {
  //     self->roomsize1 = 1;
  //     self->damp1 = 0;
  //     self->gain = FV_MUTED;
  //   } else {
  self->roomsize1 = self->roomsize;
  self->damp1 = self->damp;
  self->gain = FV_FIXEDGAIN;
  //}

  for (i = 0; i < FV_NUMCOMBS; i++) {
    FV_Comb_setfeedback(&self->combL[i], self->roomsize1);
    FV_Comb_setfeedback(&self->combR[i], self->roomsize1);
    FV_Comb_setdamp(&self->combL[i], self->damp1);
    FV_Comb_setdamp(&self->combR[i], self->damp1);
  }
}

void FV_Reverb_setroomsize(FV_Reverb *self, int32_t value) {
  self->roomsize = q16_16_multiply(value, FV_SCALEROOM) + FV_OFFSETROOM;
}

void FV_Reverb_setdamp(FV_Reverb *self, int32_t value) {
  self->damp = q16_16_multiply(value, FV_SCALEDAMP);
}

void FV_Reverb_setwet(FV_Reverb *self, int32_t value) {
  self->wet = q16_16_multiply(value, FV_SCALEWET);
}

void FV_Reverb_setdry(FV_Reverb *self, int32_t value) {
  self->dry = q16_16_multiply(value, FV_SCALEDRY);
}

void FV_Reverb_setwidth(FV_Reverb *self, int32_t value) { self->width = value; }

void FV_Reverb_setmode(FV_Reverb *self, int32_t value) { self->mode = value; }

void FV_Reverb_init(FV_Reverb *self) {
  for (int i = 0; i < FV_NUMCOMBS; i++) {
    FV_Comb_init(&self->combL[i]);
    FV_Comb_init(&self->combR[i]);
  }

  FV_Comb_setbuffer(&self->combL[0], self->bufcombL1, FV_COMBTUNINGL1);
  FV_Comb_setbuffer(&self->combR[0], self->bufcombR1, FV_COMBTUNINGR1);
  FV_Comb_setbuffer(&self->combL[1], self->bufcombL2, FV_COMBTUNINGL2);
  FV_Comb_setbuffer(&self->combR[1], self->bufcombR2, FV_COMBTUNINGR2);
  FV_Comb_setbuffer(&self->combL[2], self->bufcombL3, FV_COMBTUNINGL3);
  FV_Comb_setbuffer(&self->combR[2], self->bufcombR3, FV_COMBTUNINGR3);
  //   FV_Comb_setbuffer(&self->combL[3], self->bufcombL4, FV_COMBTUNINGL4);
  //   FV_Comb_setbuffer(&self->combR[3], self->bufcombR4, FV_COMBTUNINGR4);
  //   FV_Comb_setbuffer(&self->combL[4], self->bufcombL5, FV_COMBTUNINGL5);
  //   FV_Comb_setbuffer(&self->combR[4], self->bufcombR5, FV_COMBTUNINGR5);
  //   FV_Comb_setbuffer(&self->combL[5], self->bufcombL6, FV_COMBTUNINGL6);
  //   FV_Comb_setbuffer(&self->combR[5], self->bufcombR6, FV_COMBTUNINGR6);
  //   FV_Comb_setbuffer(&self->combL[6], self->bufcombL7, FV_COMBTUNINGL7);
  //   FV_Comb_setbuffer(&self->combR[6], self->bufcombR7, FV_COMBTUNINGR7);
  //   FV_Comb_setbuffer(&self->combL[7], self->bufcombL8, FV_COMBTUNINGL8);
  //   FV_Comb_setbuffer(&self->combR[7], self->bufcombR8, FV_COMBTUNINGR8);

  for (int i = 0; i < FV_NUMALLPASSES; i++) {
    FV_AllPass_init(&self->allpassL[i]);
    FV_AllPass_init(&self->allpassR[i]);
  }

  FV_AllPass_setbuffer(&self->allpassL[0], self->bufallpassL1,
                       FV_ALLPASSTUNINGL1);
  FV_AllPass_setbuffer(&self->allpassR[0], self->bufallpassR1,
                       FV_ALLPASSTUNINGR1);
  FV_AllPass_setbuffer(&self->allpassL[1], self->bufallpassL2,
                       FV_ALLPASSTUNINGL2);
  FV_AllPass_setbuffer(&self->allpassR[1], self->bufallpassR2,
                       FV_ALLPASSTUNINGR2);
  FV_AllPass_setbuffer(&self->allpassL[2], self->bufallpassL3,
                       FV_ALLPASSTUNINGL3);
  FV_AllPass_setbuffer(&self->allpassR[2], self->bufallpassR3,
                       FV_ALLPASSTUNINGR3);
  //   FV_AllPass_setbuffer(&self->allpassL[3], self->bufallpassL4,
  //                        FV_ALLPASSTUNINGL4);
  //   FV_AllPass_setbuffer(&self->allpassR[3], self->bufallpassR4,
  //                        FV_ALLPASSTUNINGR4);

  FV_Reverb_setroomsize(self, FV_INITIALROOM);
  FV_Reverb_setdamp(self, FV_INITIALDAMP);
  FV_Reverb_setwet(self, FV_INITIALWET);
  FV_Reverb_setdry(self, FV_INITIALDRY);
  FV_Reverb_setwidth(self, FV_INITIALWIDTH);
  FV_Reverb_setmode(self, FV_INITIALMODE);
  FV_Reverb_update(self);

  FV_Reverb_mute(self);
}

void FV_Reverb_process(FV_Reverb *self, int32_t *buf, unsigned int nr_samples) {
  int32_t outL, outR, inputL, inputR;
  for (int i = 0; i < nr_samples; i++) {
    outL = outR = 0;
    // convert int32_t to float
    inputL = q16_16_multiply(buf[2 * i + 0], self->gain);
    inputR = q16_16_multiply(buf[2 * i + 1], self->gain);

    // accumluate comb filters in parallel
    for (int j = 0; j < FV_NUMCOMBS; j++) {
      outL += FV_Comb_process(&self->combL[j], inputL);
      outR += FV_Comb_process(&self->combR[j], inputR);
    }

    // feed through allpasses in series
    for (int j = 0; j < FV_NUMALLPASSES; j++) {
      outL = FV_AllPass_process(&self->allpassL[j], outL);
      outR = FV_AllPass_process(&self->allpassR[j], outR);
    }

    // calculate output mixing with anything already there
    buf[2 * i + 0] = q16_16_multiply(buf[2 * i + 0], self->dry) +
                     q16_16_multiply(outL, self->wet);
    buf[2 * i + 1] = q16_16_multiply(buf[2 * i + 1], self->dry) +
                     q16_16_multiply(outR, self->wet);
  }
}
