// Copyright 2023-2025 Zack Scholl, GPLv3.0

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define NOSDCARD 1
#include "../../sequencer.h"

void sequencer_emit(uint8_t key) { printf("key %d\n", key); }
void sequencer_stop() { printf("stop\n"); }
void sequencer_emit2(uint8_t key) { printf("key2 %d\n", key); }
void sequencer_stop2() { printf("stop2\n"); }

void test1() {
  Sequencer *seq = Sequencer_malloc();
  Sequencer_set_callbacks(seq, sequencer_emit, sequencer_stop);
  Sequencer_add(seq, 1, 0);
  Sequencer_add(seq, 2, 3);
  Sequencer_add(seq, 3, 6);
  Sequencer_add(seq, 4, 9);
  Sequencer_add(seq, 5, 12);

  Sequencer_play(seq, false);
  for (int i = 0; i < 21; i++) {
    printf("[%d] ", i);
    Sequencer_step(seq, i);
    if (!Sequencer_is_playing(seq)) {
      Sequencer_continue(seq);
    }
  }
  printf("\n");

  Sequencer_quantize(seq, 4);
  Sequencer_play(seq, true);
  for (int i = 0; i < 30; i++) {
    printf("[%d] ", i);
    Sequencer_step(seq, i);
  }

  Sequencer_free(seq);
}

void test2() {
  Sequencer *seq = Sequencer_malloc();
  Sequencer_set_callbacks(seq, sequencer_emit, sequencer_stop);
  Sequencer_add(seq, 1, 0);
  Sequencer_add(seq, 2, 2);
  Sequencer_add(seq, 3, 4);
  Sequencer_add(seq, 99, 6);

  Sequencer *seq2 = Sequencer_malloc();
  Sequencer_set_callbacks(seq2, sequencer_emit2, sequencer_stop2);
  Sequencer_add(seq2, 4, 0);
  Sequencer_add(seq2, 5, 3);
  Sequencer_add(seq2, 6, 6);
  Sequencer_add(seq2, 99, 9);

  Sequencer_play(seq, true);
  for (int i = 0; i < 12; i++) {
    printf("[%d] ", i);
    Sequencer_step(seq, i);
  }
  printf("\n----------\n");

  Sequencer_play(seq2, true);
  for (int i = 0; i < 12; i++) {
    printf("[%d] ", i);
    Sequencer_step(seq2, i);
  }
  printf("\n----------\n");

  Sequencer *seq3 = Sequencer_merge(seq, seq2);
  Sequencer_copy(seq3, seq);
  Sequencer_free(seq3);
  Sequencer_play(seq, true);
  for (int i = 0; i < 30; i++) {
    printf("[%d] ", i);
    Sequencer_step(seq, i);
  }
  printf("\n----------\n");

  Sequencer_play(seq, false);
  for (int i = 0; i < 21; i++) {
    printf("[%d] ", i);
    Sequencer_step(seq, i);
    if (!Sequencer_is_playing(seq)) {
      Sequencer_continue(seq);
    }
  }
  printf("\n");

  Sequencer_free(seq);
  Sequencer_free(seq2);
}

void test3() {
  Sequencer *seq = Sequencer_malloc();
  Sequencer_set_callbacks(seq, sequencer_emit, sequencer_stop);
  Sequencer_add(seq, 1, 0);
  Sequencer_add(seq, 2, 2);
  Sequencer_play(seq, true);
  for (int i = 0; i < 30; i++) {
    printf("[%d] ", i);
    Sequencer_step(seq, i);
  }
  printf("len: %d\n", seq->rec_len);
  Sequencer_free(seq);
}
int main() {
  test3();
  return 0;
}
