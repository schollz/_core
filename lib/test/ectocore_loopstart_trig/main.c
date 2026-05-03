#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "../../ectocore_loopstart_trig.h"

static void test_playback_start_waits_for_slice_zero(void) {
  EctoLoopstartTrigState state;
  ecto_loopstart_trig_state_init(&state);

  assert(ecto_loopstart_trig_step(&state, 1, false, 3, 8, true) ==
         ECTO_LOOPSTART_TRIG_NONE);
  assert(state.pending);

  EctoLoopstartTrigEvent event =
      ecto_loopstart_trig_step(&state, 1, false, 0, 8, true);
  assert(event == ECTO_LOOPSTART_TRIG_PLAYBACK_START);
  ecto_loopstart_trig_mark_emitted(&state, event);
  assert(!state.pending);

  assert(ecto_loopstart_trig_step(&state, 1, false, 0, 8, true) ==
         ECTO_LOOPSTART_TRIG_NONE);
}

static void test_wrap_from_last_slice_to_zero(void) {
  EctoLoopstartTrigState state;
  ecto_loopstart_trig_state_init(&state);

  EctoLoopstartTrigEvent event =
      ecto_loopstart_trig_step(&state, 1, false, 0, 8, true);
  assert(event == ECTO_LOOPSTART_TRIG_PLAYBACK_START);
  ecto_loopstart_trig_mark_emitted(&state, event);

  assert(ecto_loopstart_trig_step(&state, 1, false, 7, 8, true) ==
         ECTO_LOOPSTART_TRIG_NONE);
  assert(ecto_loopstart_trig_step(&state, 1, false, 0, 8, true) ==
         ECTO_LOOPSTART_TRIG_WRAP);
  assert(ecto_loopstart_trig_step(&state, 1, false, 0, 8, true) ==
         ECTO_LOOPSTART_TRIG_NONE);
}

static void test_transient_start_window(void) {
  assert(ecto_loopstart_trig_transient_pos_is_start(0));
  assert(ecto_loopstart_trig_transient_pos_is_start(1));
  assert(ecto_loopstart_trig_transient_pos_is_start(
      ECTO_LOOP_START_TRANSIENT_MAX));
  assert(!ecto_loopstart_trig_transient_pos_is_start(
      ECTO_LOOP_START_TRANSIENT_MAX + 1));
}

static void test_random_mode_does_not_emit(void) {
  EctoLoopstartTrigState state;
  ecto_loopstart_trig_state_init(&state);

  assert(ecto_loopstart_trig_step(&state, 1, false, 0, 8, false) ==
         ECTO_LOOPSTART_TRIG_NONE);
  assert(ecto_loopstart_trig_step(&state, 1, false, 7, 8, false) ==
         ECTO_LOOPSTART_TRIG_NONE);
  assert(ecto_loopstart_trig_step(&state, 1, false, 0, 8, false) ==
         ECTO_LOOPSTART_TRIG_NONE);
}

static void test_sample_change_and_invalid_state_clear_pending(void) {
  EctoLoopstartTrigState state;
  ecto_loopstart_trig_state_init(&state);

  assert(ecto_loopstart_trig_step(&state, 1, false, 3, 8, true) ==
         ECTO_LOOPSTART_TRIG_NONE);
  assert(state.pending);

  assert(ecto_loopstart_trig_step(&state, 2, false, 0, 8, true) ==
         ECTO_LOOPSTART_TRIG_NONE);
  assert(!state.pending);

  assert(ecto_loopstart_trig_step(&state, 2, true, 0, 8, true) ==
         ECTO_LOOPSTART_TRIG_NONE);
  assert(ecto_loopstart_trig_step(&state, 2, false, 3, 8, true) ==
         ECTO_LOOPSTART_TRIG_NONE);
  assert(state.pending);

  assert(ecto_loopstart_trig_step(&state, 0, false, 0, 0, true) ==
         ECTO_LOOPSTART_TRIG_NONE);
  assert(!state.pending);
}

int main(void) {
  test_playback_start_waits_for_slice_zero();
  test_wrap_from_last_slice_to_zero();
  test_transient_start_window();
  test_random_mode_does_not_emit();
  test_sample_change_and_invalid_state_clear_pending();
  return 0;
}
