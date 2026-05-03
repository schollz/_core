// Copyright 2023-2025 Zack Scholl, GPLv3.0

#ifndef ECTOCORE_LOOPSTART_TRIG_H
#define ECTOCORE_LOOPSTART_TRIG_H 1

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ECTO_LOOP_START_TRANSIENT_MAX 28
#define ECTO_LOOP_START_TRIG_DUP_SUPPRESS_MS 8

typedef enum EctoLoopstartTrigEvent {
  ECTO_LOOPSTART_TRIG_NONE = 0,
  ECTO_LOOPSTART_TRIG_PLAYBACK_START,
  ECTO_LOOPSTART_TRIG_WRAP,
} EctoLoopstartTrigEvent;

typedef struct EctoLoopstartTrigState {
  uint8_t last_slice;
  uint8_t last_slice_num;
  bool prev_playback_stopped;
  bool pending;
  uintptr_t sample_info_id;
} EctoLoopstartTrigState;

static inline void ecto_loopstart_trig_state_init(
    EctoLoopstartTrigState *state) {
  state->last_slice = 255;
  state->last_slice_num = 0;
  state->prev_playback_stopped = true;
  state->pending = false;
  state->sample_info_id = 0;
}

static inline bool ecto_loopstart_trig_transient_pos_is_start(
    uint16_t transient_pos) {
  // Transient positions are stored divided by 16, so a counted value of 0 can
  // represent an onset in the first 16 samples.
  return transient_pos <= ECTO_LOOP_START_TRANSIENT_MAX;
}

static inline EctoLoopstartTrigEvent ecto_loopstart_trig_step(
    EctoLoopstartTrigState *state, uintptr_t sample_info_id,
    bool playback_stopped, uint8_t current_slice, uint8_t slice_num,
    bool selected_mode_has_loop_start_transient) {
  if (state == NULL) {
    return ECTO_LOOPSTART_TRIG_NONE;
  }

  bool sample_changed =
      state->sample_info_id != 0 && state->sample_info_id != sample_info_id;
  state->sample_info_id = sample_info_id;

  if (sample_changed || sample_info_id == 0 || slice_num == 0 ||
      current_slice >= slice_num) {
    state->pending = false;
    state->last_slice = 255;
    state->last_slice_num = 0;
    state->prev_playback_stopped = playback_stopped;
    return ECTO_LOOPSTART_TRIG_NONE;
  }

  bool playback_started_now = state->prev_playback_stopped && !playback_stopped;
  bool strict_loop_wrap = false;

  if (playback_started_now) {
    state->pending = true;
  }

  if (playback_stopped) {
    state->pending = false;
  }

  if (state->last_slice_num == slice_num && state->last_slice < slice_num) {
    strict_loop_wrap =
        (state->last_slice == slice_num - 1) && (current_slice == 0);
  }

  EctoLoopstartTrigEvent event = ECTO_LOOPSTART_TRIG_NONE;
  if (!playback_stopped && selected_mode_has_loop_start_transient) {
    if (state->pending && current_slice == 0) {
      event = ECTO_LOOPSTART_TRIG_PLAYBACK_START;
    } else if (strict_loop_wrap) {
      event = ECTO_LOOPSTART_TRIG_WRAP;
    }
  }

  state->last_slice = current_slice;
  state->last_slice_num = slice_num;
  state->prev_playback_stopped = playback_stopped;
  return event;
}

static inline void ecto_loopstart_trig_mark_emitted(
    EctoLoopstartTrigState *state, EctoLoopstartTrigEvent event) {
  if (state != NULL && event != ECTO_LOOPSTART_TRIG_NONE) {
    state->pending = false;
  }
}

#endif
