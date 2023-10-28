#ifndef CHAIN_LIB
#define CHAIN_LIB 1

#define CHAIN_MAX_SEQUENCES 16
#define CHAIN_MAX_LINKS 255

#include "sequencer.h"

typedef struct Chain {
  Sequencer *seq[CHAIN_MAX_SEQUENCES];

  // recording the links
  uint8_t rec_cur;
  uint8_t rec_seq[CHAIN_MAX_LINKS];
  uint8_t rec_len;
} Chain;

void *Chain_reset(Chain *c) { c->rec_cur = 0; }

void *Chain_clear(Chain *c) {
  c->rec_cur = 0;
  c->rec_len = 0;
}

Chain *Chain_create() {
  Chain *c = (Chain *)malloc(sizeof(Chain));
  for (uint8_t i = 0; i < CHAIN_MAX_SEQUENCES; i++) {
    c->seq[i] = Sequencer_create();
  }
  Chain_clear(c);
  return c;
}

void Chain_link(Chain *c, uint8_t *seq, uint16_t len) {
  if (len > CHAIN_MAX_LINKS) {
    len = CHAIN_MAX_LINKS;
  }
  for (uint8_t i = 0; i < len; i++) {
    c->rec_seq[i] = seq[i];
  }
  c->rec_len = len;
}

void Chain_add(Chain *c, uint8_t seq, uint8_t key, uint32_t step) {
  Sequencer_add(c->seq[seq], key, step);
}

int8_t Chain_emit(Chain *c, uint32_t step) {
  int8_t beat = Sequencer_emit(c->seq[c->rec_seq[c->rec_cur]], step);
  if (beat == -1 && Sequencer_is_finished(c->seq[c->rec_seq[c->rec_cur]])) {
    if (c->rec_len > 0) {
      c->rec_cur++;
      if (c->rec_cur > c->rec_len) {
        c->rec_cur = 0;
      }
    }
  }
  return beat;
}

uint8_t Chain_get_current(Chain *c) { return c->rec_seq[c->rec_cur]; }

#endif