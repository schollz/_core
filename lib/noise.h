// Copyright 2023-2025 Zack Scholl, GPLv3.0

#ifndef NOISE_LIB

typedef struct Noise {
  int sck;
  uint32_t mSampleRate;
  float m_nextmidpt;
  float m_nextvalue;
  float level;
  float curve;
  float slope;
  int counter;
  uint8_t counter_steps;
  uint32_t s1, s2, s3;  // random generator state
} Noise;

// Helper function declarations
uint32_t trand(Noise *noise);
float frand2(Noise *noise);
void fseed(Noise *noise, uint32_t seed);

Noise *Noise_create(uint32_t seed, uint32_t mSampleRate);
uint32_t RandUint32(Noise *noise);
float Range(float f, float dlo, float dhi);
float LFNoise0(Noise *noise, int32_t freq);
float LFNoise0_seeded(Noise *noise, float freq, uint8_t steps, uint32_t seed);
float LFNoise2(Noise *noise, int32_t freq);

uint32_t trand(Noise *noise) {
  noise->s1 = ((noise->s1 & (uint32_t)-2) << 12) ^
              (((noise->s1 << 13) ^ noise->s1) >> 19);
  noise->s2 = ((noise->s2 & (uint32_t)-8) << 4) ^
              (((noise->s2 << 2) ^ noise->s2) >> 25);
  noise->s3 = ((noise->s3 & (uint32_t)-16) << 17) ^
              (((noise->s3 << 3) ^ noise->s3) >> 11);
  return noise->s1 ^ noise->s2 ^ noise->s3;
}

float frand2(Noise *noise) {
  union {
    uint32_t i;
    float f;
  } u;  // union for floating point conversion of result
  u.i = 0x40000000 | (trand(noise) >> 9);
  return u.f - 3.f;
}

void fseed(Noise *noise, uint32_t seed) {
  // initialize seeds using the given seed value taking care of
  // the requirements. The constants below are arbitrary otherwise
  noise->s1 = 1243598713U ^ seed;
  if (noise->s1 < 2) noise->s1 = 1243598713U;
  noise->s2 = 3093459404U ^ seed;
  if (noise->s2 < 8) noise->s2 = 3093459404U;
  noise->s3 = 1821928721U ^ seed;
  if (noise->s3 < 16) noise->s3 = 1821928721U;
}

Noise *Noise_create(uint32_t seed, uint32_t mSampleRate) {
  Noise *noise = (Noise *)malloc(sizeof(Noise));
  noise->mSampleRate = mSampleRate;  // hz
  noise->counter = 0;
  noise->m_nextmidpt = 0;
  noise->m_nextvalue = 0;
  noise->level = 0;
  noise->slope = 0;
  noise->counter_steps = 0;
  fseed(noise, seed);
  return noise;
}

uint32_t RandUint32(Noise *noise) { return noise->s1 + noise->s2 + noise->s3; }

float Range(float f, float dlo, float dhi) {
  return (f + 1) / 2 * (dhi - dlo) + dlo;
}

float LFNoise0(Noise *noise, int32_t freq) {
  if (noise->counter <= 0) {
    noise->counter = (int32_t)(noise->mSampleRate / freq);
    noise->level = frand2(noise);
  }
  noise->counter -= 1;
  return noise->level;
}

float LFNoise0_seeded(Noise *noise, float freq, uint8_t steps, uint32_t seed) {
  if (noise->counter <= 0) {
    if (noise->counter_steps == 0) {
      fseed(noise, seed);
    }
    noise->counter = (int32_t)(noise->mSampleRate / freq);
    noise->level = frand2(noise);
    noise->counter_steps++;
    if (noise->counter_steps == steps) {
      noise->counter_steps = 0;
    }
  }
  noise->counter -= 1;
  return noise->level;
}

float LFNoise2(Noise *noise, int32_t freq) {
  if (noise->counter <= 0) {
    float value = noise->m_nextvalue;
    noise->m_nextvalue = frand2(noise);
    noise->level = noise->m_nextmidpt;
    noise->m_nextmidpt = (noise->m_nextvalue + value) * 0.5;
    noise->counter = (int32_t)(noise->mSampleRate / freq);
    float fseglen = (float)noise->counter;
    noise->curve =
        2.0 * (noise->m_nextmidpt - noise->level - fseglen * noise->slope) /
        (fseglen * fseglen + fseglen);
  }
  noise->counter -= 1;
  noise->slope += noise->curve;
  noise->level += noise->slope;
  return noise->level;
}

float LFNoise2_period(Noise *noise, int32_t freqmult) {
  if (noise->counter <= 0) {
    float value = noise->m_nextvalue;
    noise->m_nextvalue = frand2(noise);
    noise->level = noise->m_nextmidpt;
    noise->m_nextmidpt = (noise->m_nextvalue + value) * 0.5;
    noise->counter = (int32_t)(noise->mSampleRate * freqmult);
    float fseglen = (float)noise->counter;
    noise->curve =
        2.0 * (noise->m_nextmidpt - noise->level - fseglen * noise->slope) /
        (fseglen * fseglen + fseglen);
  }
  noise->counter -= 1;
  noise->slope += noise->curve;
  noise->level += noise->slope;
  return noise->level;
}

void Noise_destroy(Noise *noise) { free(noise); }

#endif /* NOISE_LIB */
#define NOISE_LIB 1