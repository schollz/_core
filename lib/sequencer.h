// Copyright 2023 Zack Scholl.
//
// Author: Zack Scholl (zack.scholl@gmail.com)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// See http://creativecommons.org/licenses/MIT/ for more information.

#ifndef SEQUENCER_LIB
#define SEQUENCER_LIB 1

#include "utils.h"

#define SEQUENCER_MAX_STEPS 32
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
  uint32_t rec_step_offset;

  // quantization
  uint8_t quantization;  // 1, 6, 12, 24, 48, 96, 192

  // playing data
  uint8_t play_pos;
  uint16_t play_step;
  bool play_finished;

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
  seq->play_finished = true;
  seq->quantization = 1;
}

Sequencer *Sequencer_malloc() {
  Sequencer *seq = (Sequencer *)malloc(sizeof(Sequencer));
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

bool Sequencer_has_data(Sequencer *seq) { return seq->rec_len > 0; }

// Sequencer_add adds a key and step to the sequencer.
// The start of the sequence is always 0 and the last item of the sequence
// is not played.
uint16_t Sequencer_add(Sequencer *seq, uint8_t key, uint32_t step) {
  if (seq->rec_len < SEQUENCER_MAX_STEPS) {
    if (seq->rec_len == 0) {
      seq->rec_steps[seq->rec_len] = 0;
    } else {
      seq->rec_steps[seq->rec_len] = (step - seq->rec_step_offset);
    }
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

void Sequencer_play(Sequencer *seq) {
  seq->play_pos = 0;
  seq->play_step = 0;
  seq->play_finished = false;
}

void Sequencer_step(Sequencer *seq, uint32_t step) {
  if (seq->rec_len == 0 || seq->play_finished) {
    return;
  }
  if (seq->play_step >=
      round_uint16_to(seq->rec_steps[seq->play_pos], seq->quantization)) {
    if (seq->play_pos >= seq->rec_len - 1) {
      seq->play_finished = true;
      seq->play_pos = 0;
      seq->play_step = 0;
      if (seq->sequence_finished != NULL) {
        seq->sequence_finished();
      }
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

bool Sequencer_is_finished(Sequencer *seq) { return seq->play_finished; }
void Sequencer_stop(Sequencer *seq) { seq->play_finished = true; }

#endif