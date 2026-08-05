#ifndef LIB_BREAK_KNOB_MAPPING_H
#define LIB_BREAK_KNOB_MAPPING_H 1

#include <stdbool.h>
#include <stdint.h>

#define GRIMOIRE_EFFECT_COUNT 16
#define GRIMOIRE_EFFECT_NONE (-1)

typedef enum GrimoireEffect {
  GRIMOIRE_EFFECT_DISTORTION = 0,
  GRIMOIRE_EFFECT_LOSS,
  GRIMOIRE_EFFECT_BITCRUSH,
  GRIMOIRE_EFFECT_FILTER,
  GRIMOIRE_EFFECT_STRETCH,
  GRIMOIRE_EFFECT_DELAY,
  GRIMOIRE_EFFECT_COMB,
  GRIMOIRE_EFFECT_BEAT_REPEAT,
  GRIMOIRE_EFFECT_REVERB,
  GRIMOIRE_EFFECT_AUTOPAN,
  GRIMOIRE_EFFECT_PITCH_DOWN,
  GRIMOIRE_EFFECT_PITCH_UP,
  GRIMOIRE_EFFECT_REVERSE,
  GRIMOIRE_EFFECT_RETRIGGER,
  GRIMOIRE_EFFECT_RETRIGGER_PITCHED,
  GRIMOIRE_EFFECT_TAPE_STOP,
} GrimoireEffect;

static inline int8_t grimoire_find_single_effect(const bool *effects) {
  int8_t selected = GRIMOIRE_EFFECT_NONE;
  for (uint8_t effect = 0; effect < GRIMOIRE_EFFECT_COUNT; effect++) {
    if (!effects[effect]) {
      continue;
    }
    if (selected != GRIMOIRE_EFFECT_NONE) {
      return GRIMOIRE_EFFECT_NONE;
    }
    selected = (int8_t)effect;
  }
  return selected;
}

static inline uint16_t break_direct_clamp(int16_t value) {
  if (value <= 0) {
    return 0;
  }
  if (value >= 1024) {
    return 1024;
  }
  return (uint16_t)value;
}

static inline uint8_t break_direct_u8(uint16_t value) {
  if (value >= 1024) {
    return 255;
  }
  return (uint8_t)(((uint32_t)value * 255u) / 1024u);
}

static inline uint8_t break_direct_retrigger_division(uint8_t amount) {
  static const uint8_t divisions[8] = {1, 2, 3, 4, 6, 8, 12, 16};
  uint8_t index = (uint8_t)(((uint16_t)amount * 8u) >> 8u);
  if (index > 7) {
    index = 7;
  }
  return divisions[index];
}

#endif
