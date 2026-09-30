#ifndef CORE_MIDI_CHANNEL_H
#define CORE_MIDI_CHANNEL_H
#include <stddef.h>
#include <stdint.h>

// Card values are canonical decimal channels with optional trailing whitespace.
// Zero means invalid; callers choose whether to warn or use channel 1.
static inline uint8_t midi_channel_parse(const char *value, size_t length) {
  while (length && (value[length - 1] == ' ' || value[length - 1] == '\t' ||
         value[length - 1] == '\r' || value[length - 1] == '\n' ||
         value[length - 1] == '\v' || value[length - 1] == '\f')) --length;
  if (length == 1 && value[0] >= '1' && value[0] <= '9')
    return (uint8_t)(value[0] - '0');
  if (length == 2 && value[0] == '1' && value[1] >= '0' && value[1] <= '6')
    return (uint8_t)(10 + value[1] - '0');
  return 0;
}
#endif
