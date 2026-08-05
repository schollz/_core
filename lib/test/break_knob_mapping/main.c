#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "../../break_knob_mapping.h"

static void test_single_effect_detection(void) {
  bool effects[GRIMOIRE_EFFECT_COUNT] = {false};
  assert(grimoire_find_single_effect(effects) == GRIMOIRE_EFFECT_NONE);

  for (uint8_t selected = 0; selected < GRIMOIRE_EFFECT_COUNT; selected++) {
    for (uint8_t effect = 0; effect < GRIMOIRE_EFFECT_COUNT; effect++) {
      effects[effect] = effect == selected;
    }
    assert(grimoire_find_single_effect(effects) == selected);
  }

  effects[0] = true;
  effects[15] = true;
  assert(grimoire_find_single_effect(effects) == GRIMOIRE_EFFECT_NONE);
}

static void test_control_scaling(void) {
  assert(break_direct_clamp(-100) == 0);
  assert(break_direct_clamp(0) == 0);
  assert(break_direct_clamp(512) == 512);
  assert(break_direct_clamp(1024) == 1024);
  assert(break_direct_clamp(1200) == 1024);

  assert(break_direct_u8(0) == 0);
  assert(break_direct_u8(512) == 127);
  assert(break_direct_u8(1024) == 255);
}

static void test_rhythmic_divisions(void) {
  const uint8_t expected[8] = {1, 2, 3, 4, 6, 8, 12, 16};
  for (uint8_t index = 0; index < 8; index++) {
    assert(break_direct_retrigger_division(index * 32) == expected[index]);
  }
  assert(break_direct_retrigger_division(255) == 16);
}

int main(void) {
  test_single_effect_detection();
  test_control_scaling();
  test_rhythmic_divisions();
  return 0;
}
