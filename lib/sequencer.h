// Copyright 2023-2025 Zack Scholl, GPLv3.0

#ifndef SEQUENCER_LIB
#define SEQUENCER_LIB 1

#include "utils.h"

#define SEQUENCER_MAX_STEPS 64
#define SEQUENCER_FINISHED -2

uint16_t round_uint16_to(uint16_t num, uint16_t multiple) {
  if (multiple <= 1 || num == 0) {
    return num;
  }
  num = (((2 * num) + multiple) / (2 * multiple)) * multiple;
  if (num == 0) {
    num = multiple;
  }
  return num;
}

typedef struct Sequencer {
  // stored data
  uint8_t rec_len;
  uint8_t rec_key[SEQUENCER_MAX_STEPS];
  uint16_t rec_steps[SEQUENCER_MAX_STEPS];
  int64_t rec_step_offset;

  // quantization
  uint8_t quantization;  // 1, 6, 12, 24, 48, 96, 192

  // playing data
  uint8_t play_pos;
  uint16_t play_step : 14;
  uint16_t is_playing : 1;
  uint16_t is_repeating : 1;

  // callbacks
  callback_uint8 sequence_emit;
  callback_void sequence_finished;
} Sequencer;

void Sequencer_clear(Sequencer *seq) {
  seq->rec_len = 0;
  seq->rec_step_offset = 0;
  for (uint8_t i = 0; i < SEQUENCER_MAX_STEPS; i++) {
    seq->rec_key[i] = 0;
    seq->rec_steps[i] = 0;
  }
  seq->is_playing = 0;
  seq->is_repeating = 0;
  seq->quantization = 1;
  seq->play_pos = 0;
  seq->play_step = 0;
}

Sequencer *Sequencer_malloc() {
  Sequencer *seq = (Sequencer *)malloc(sizeof(Sequencer));
  seq->quantization = 1;
  seq->sequence_emit = NULL;
  seq->sequence_finished = NULL;
  Sequencer_clear(seq);
  return seq;
}

void Sequencer_free(Sequencer *seq) { free(seq); }

void Sequencer_set_callbacks(Sequencer *seq, callback_uint8 sequence_emit,
                             callback_void sequence_finished) {
  seq->sequence_emit = sequence_emit;
  seq->sequence_finished = sequence_finished;
}

bool Sequencer_has_data(Sequencer *seq) { return seq->rec_len > 1; }

// Sequencer_add adds a key and step to the sequencer.
// The start of the sequence is always 0 and the last item of the sequence
// is not played.
uint16_t Sequencer_add(Sequencer *seq, uint8_t key, int64_t step) {
  if (seq->rec_len < SEQUENCER_MAX_STEPS) {
    if (seq->rec_len == 0) {
      seq->rec_steps[seq->rec_len] = 0;
    } else {
      seq->rec_steps[seq->rec_len] = (step - seq->rec_step_offset);
    }
    printf("[sequencer] step %d: %d\n", seq->rec_len,
           seq->rec_steps[seq->rec_len]);
    seq->rec_key[seq->rec_len] = key;
    seq->rec_step_offset = step;
    seq->rec_len++;
    return seq->rec_steps[seq->rec_len - 1];
  }
  return 0;
}

void Sequencer_print(Sequencer *seq) {
  uint32_t step = 0;
  for (uint16_t i = 0; i < seq->rec_len; i++) {
    printf("%d) %d -> key %d\n", i,
           step + round_uint16_to(seq->rec_steps[i], seq->quantization),
           seq->rec_key[i]);
    step += round_uint16_to(seq->rec_steps[i], seq->quantization);
  }
}
void Sequencer_quantize(Sequencer *seq, uint8_t quantization) {
  seq->quantization = quantization;
}

void Sequencer_play(Sequencer *seq, bool do_repeat) {
  seq->play_pos = 0;
  seq->play_step = 0;
  seq->is_playing = 1;
  seq->is_repeating = do_repeat;
}

bool Sequencer_is_playing(Sequencer *seq) { return seq->is_playing; }
void Sequencer_stop(Sequencer *seq) {
  seq->is_playing = false;
  seq->play_pos = 0;
  seq->play_step = 0;
  if (seq->sequence_finished != NULL) {
    seq->sequence_finished();
  }
}

void Sequencer_continue(Sequencer *seq) {
  if (seq->sequence_emit != NULL) {
    seq->sequence_emit(seq->rec_key[0]);
  }
  seq->play_pos = 1;
  seq->play_step = 1;
  seq->is_playing = 1;
}

void Sequencer_copy(Sequencer *seq, Sequencer *copy) {
  copy->rec_len = seq->rec_len;
  copy->rec_step_offset = seq->rec_step_offset;
  for (uint8_t i = 0; i < seq->rec_len; i++) {
    copy->rec_key[i] = seq->rec_key[i];
    copy->rec_steps[i] = seq->rec_steps[i];
  }
  copy->is_playing = 0;
  copy->is_repeating = 0;
  copy->quantization = seq->quantization;
  copy->play_pos = 0;
  copy->play_step = 0;
  copy->sequence_emit = seq->sequence_emit;
  copy->sequence_finished = seq->sequence_finished;
}

Sequencer *Sequencer_merge(Sequencer *seq, Sequencer *other) {
  Sequencer *merged = (Sequencer *)malloc(sizeof(Sequencer));
  Sequencer_copy(seq, merged);
  if (other->rec_len == 0) {
    return merged;
  }
  // merge them togehter
  other->rec_steps[0] = seq->rec_steps[seq->rec_len - 1];
  for (uint8_t i = 0; i < other->rec_len; i++) {
    if (merged->rec_len < SEQUENCER_MAX_STEPS) {
      merged->rec_steps[merged->rec_len - 1] = other->rec_steps[i];
      merged->rec_key[merged->rec_len - 1] = other->rec_key[i];
      merged->rec_len++;
    } else {
      break;
    }
  }
  merged->rec_len--;
  return merged;
}

void Sequencer_step(Sequencer *seq, int64_t step) {
  if (seq->rec_len == 0 || !seq->is_playing) {
    return;
  }
  if (seq->play_step >=
      round_uint16_to(seq->rec_steps[seq->play_pos], seq->quantization)) {
    if (seq->play_pos >= seq->rec_len - 1) {
      if (seq->is_repeating) {
        Sequencer_continue(seq);
      } else {
        Sequencer_stop(seq);
      }
      return;
    } else {
      if (seq->sequence_emit != NULL) {
        seq->sequence_emit(seq->rec_key[seq->play_pos]);
      }
    }
    seq->play_pos++;
    seq->play_step = 0;
  }
  seq->play_step++;
}

#endif