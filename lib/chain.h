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

#ifndef CHAIN_LIB
#define CHAIN_LIB 1

#define CHAIN_MAX_SEQUENCES 16
#define CHAIN_MAX_LINKS 255

#include "sequencer.h"

typedef struct Chain {
  Sequencer seq[CHAIN_MAX_SEQUENCES];

  // recording the links
  uint8_t rec_cur;
  uint8_t rec_seq[CHAIN_MAX_LINKS];
  uint8_t rec_last;
} Chain;

void *Chain_reset(Chain *c) { c->rec_cur = 0; }

Chain *Chain_create() {
  Chain *c = (Chain *)malloc(sizeof(Chain));
  for (uint8_t i = 0; i < CHAIN_MAX_SEQUENCES; i++) {
    Sequencer_clear(&c->seq[i]);
  }
  c->rec_cur = 0;
  c->rec_last = 0;
  return c;
}

void Chain_link(Chain *c, uint8_t *seq, uint16_t len) {
  if (len > CHAIN_MAX_LINKS) {
    len = CHAIN_MAX_LINKS;
  }
  if (len == 0) {
    return;
  }
  printf("creating link of size %d:\n", len);
  for (uint8_t i = 0; i < len; i++) {
    c->rec_seq[i] = seq[i];
    printf("%d: %d\n", i, seq[i]);
  }
  c->rec_last = len - 1;
}

void Chain_add(Chain *c, uint8_t seq, uint8_t key, uint32_t step) {
  Sequencer_add(&c->seq[seq], key, step);
}

void Chain_quantize(Chain *c, uint8_t seq, uint16_t quantization) {
  Sequencer_quantize(&c->seq[seq], quantization);
}

void Chain_add_current(Chain *c, uint8_t key, uint32_t step) {
  uint16_t step_added =
      Sequencer_add(&c->seq[c->rec_seq[c->rec_last]], key, step);
  printf("[chain] add key %d at step %d (%d) to sequence %d\n", key, step_added,
         step, c->rec_seq[c->rec_last]);
}

void Chain_clear_seq_current(Chain *c) {
  Sequencer_clear(&c->seq[c->rec_seq[c->rec_last]]);
}

void Chain_restart(Chain *c) { c->rec_cur = 0; }

int8_t Chain_emit(Chain *c, uint32_t step) {
  int8_t beat = Sequencer_emit(&c->seq[c->rec_seq[c->rec_cur]], step);
  if (beat == -1 && Sequencer_is_finished(&c->seq[c->rec_seq[c->rec_cur]])) {
    if (c->rec_last > 0) {
      c->rec_cur++;
      if (c->rec_cur > c->rec_last) {
        c->rec_cur = 0;
      }
    }
    beat = Sequencer_emit(&c->seq[c->rec_seq[c->rec_cur]], step);
  }
  return beat;
}

uint8_t Chain_get_current(Chain *c) { return c->rec_seq[c->rec_cur]; }

#ifndef NOSDCARD

bool Chain_load(Chain *c, bool *sync_sd_card) {
  while (*sync_sd_card) {
    sleep_us(100);
  }
  *sync_sd_card = true;
  FIL fil; /* File object */
  FRESULT fr;
  printf("[chain] reading\n");
  if (f_open(&fil, "chain.bin", FA_READ)) {
    printf("[chain] no save file, skipping ");
  } else {
    unsigned int bytes_read;
    fr = f_read(&fil, c, sizeof(Chain), &bytes_read);
    if (fr) {
      printf("[chain] problem reading save file: %s\n", FRESULT_str(fr));
    } else {
      printf("[chain] read %d bytes\n", bytes_read);
      printf("[chain] rec->last = %d\n", c->rec_last);
    }
  }
  f_close(&fil);
  *sync_sd_card = false;
  return true;
}

bool Chain_save(Chain *c, bool *sync_sd_card) {
  while (*sync_sd_card) {
    sleep_us(100);
  }
  *sync_sd_card = true;
  printf("[chain] writing\n");
  FRESULT fr;
  FIL file; /* File object */
  printf("[chain] opening savefile for writing\n");
  fr = f_open(&file, "chain.bin", FA_WRITE | FA_CREATE_ALWAYS);
  if (FR_OK != fr) {
    printf("f_open error: %s (%d)\n", FRESULT_str(fr), fr);
    *sync_sd_card = false;
    return false;
  }
  unsigned int bw;
  if (f_write(&file, c, sizeof(Chain), &bw)) {
    printf("[chain] problem writing save\n");
  }
  printf("[chain] wrote %d bytes\n", bw);
  f_close(&file);
  *sync_sd_card = false;
  return true;
}

#endif

#endif
