
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "../../fixedpoint.h"

/// Convert seconds to milliseconds
#define SEC_TO_MS(sec) ((sec)*1000)
/// Convert seconds to microseconds
#define SEC_TO_US(sec) ((sec)*1000000)
/// Convert seconds to nanoseconds
#define SEC_TO_NS(sec) ((sec)*1000000000)

/// Convert nanoseconds to seconds
#define NS_TO_SEC(ns) ((ns) / 1000000000)
/// Convert nanoseconds to milliseconds
#define NS_TO_MS(ns) ((ns) / 1000000)
/// Convert nanoseconds to microseconds
#define NS_TO_US(ns) ((ns) / 1000)
// https://stackoverflow.com/questions/5833094/get-a-timestamp-in-c-in-microseconds
uint64_t micros() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
  uint64_t us = SEC_TO_US((uint64_t)ts.tv_sec) + NS_TO_US((uint64_t)ts.tv_nsec);
  return us;
}

uint32_t time_ms_32() { return (micros() / 1000) % 10000000; }

enum envState { env_idle = 0, env_attack, env_decay, env_sustain, env_release };

typedef struct ADSR {
  float attack;   // milliseconds
  float decay;    // milliseconds
  float sustain;  // level
  float release;  // milliseconds
  float level;
  float level_attack;
  float level_release;
  float level_start;
  float start_time;
  float shape;
  int32_t state;
  bool gate;
} ADSR;

ADSR *ADSR_malloc(float attack, float decay, float sustain, float release,
                  float shape) {
  ADSR *adsr = (ADSR *)malloc(sizeof(ADSR));
  adsr->attack = attack;
  adsr->level_attack = 0;
  adsr->decay = decay;
  adsr->sustain = sustain;
  adsr->release = release;
  adsr->state = env_idle;
  adsr->level = 0;
  adsr->level_start = 0;
  adsr->start_time = 0;
  adsr->gate = false;
  adsr->shape = shape;
  return adsr;
}

void ADSR_free(ADSR *adsr) { free(adsr); }

void ADSR_gate(ADSR *adsr, bool gate, uint32_t current_time_ms) {
  if (adsr->gate == gate) {
    return;
  }
  adsr->gate = gate;
  adsr->start_time = current_time_ms;
  if (gate) {
    adsr->state = env_attack;
  } else {
    adsr->state = env_release;
  }
}

void ADSR_process(ADSR *adsr, float current_time_ms) {
  if (adsr->state == env_attack) {
    int32_t elapsed = current_time_ms - adsr->start_time;
    float curve_shape = adsr->attack / adsr->shape;
    adsr->level =
        adsr->level_start +
        (1.0 - adsr->level_start) * (1.0 - exp(-1.0 * (elapsed / curve_shape)));
    adsr->level_attack = adsr->level;
    adsr->level_release = adsr->level;
    if (elapsed >= adsr->attack) {
      adsr->state = env_decay;
      adsr->start_time = adsr->attack;
    }
  }

  if (adsr->state == env_decay) {
    int32_t elapsed = current_time_ms - adsr->start_time;
    if (elapsed >= adsr->decay) {
      adsr->state = env_sustain;
      adsr->start_time = current_time_ms - (elapsed - adsr->decay);
    } else {
      float curve_shape = adsr->decay / adsr->shape;
      adsr->level = adsr->sustain + (adsr->level_attack - adsr->sustain) *
                                        exp(-1.0 * (elapsed / curve_shape));
      adsr->level_release = adsr->level;
    }
  }

  if (adsr->state == env_sustain) {
    adsr->level = adsr->level_release;
  }

  if (adsr->state == env_release) {
    int32_t elapsed = current_time_ms - adsr->start_time;
    if (elapsed >= adsr->release * 2) {
      adsr->state = env_idle;
      adsr->level = 0;
    } else {
      float curve_shape = adsr->release / adsr->shape;
      adsr->level = adsr->level_release * exp(-1.0 * (elapsed / curve_shape));
    }
  }
}

float ADSR_get_level(ADSR *adsr) { return adsr->level; }

int main() {
  float attack = 300;
  float decay = 1000;
  ADSR *adsr = ADSR_malloc(attack, decay, 0.2, 1000, 4);

  ADSR_gate(adsr, true, 0);
  for (float current_time_ms = 0; current_time_ms < 3000;
       current_time_ms += 13) {
    ADSR_process(adsr, current_time_ms);
    printf("%f %f\n", current_time_ms / 1000, ADSR_get_level(adsr));
    if (current_time_ms > 1200) {
      ADSR_gate(adsr, false, current_time_ms);
    }
  }

  ADSR_free(adsr);
}
