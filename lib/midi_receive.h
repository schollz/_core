#ifndef CORE_MIDI_RECEIVE_H
#define CORE_MIDI_RECEIVE_H
#include <stdbool.h>
#include <stdint.h>
#ifdef INCLUDE_ZEPTOCORE
static uint8_t midi_receive_channel = 1;
#endif
static inline bool midi_receive_matches(uint8_t status) {
#ifdef INCLUDE_ZEPTOCORE
  return (status & 15) == midi_receive_channel - 1;
#else
  return (status & 15) == 0;
#endif
}
#endif
