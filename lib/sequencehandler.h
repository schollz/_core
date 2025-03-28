// Copyright 2023-2025 Zack Scholl, GPLv3.0

#ifndef SEQUENCERHANDLER_LIB
#define SEQUENCERHANDLER_LIB 1

typedef struct SequencerHandler {
  uint8_t playing : 7;
  uint8_t recording : 1;
} SequencerHandler;

#endif