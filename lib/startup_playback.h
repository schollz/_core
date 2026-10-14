#ifndef CORE_STARTUP_PLAYBACK_H
#define CORE_STARTUP_PLAYBACK_H
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

// Canonical startup mode, with optional trailing whitespace. False preserves
// the existing startup behavior; it is also the fallback for invalid input.
static inline bool startup_playback_parse(const char *value, size_t length,
                                          bool *wait) {
  *wait = false;
  while (length && (value[length - 1] == ' ' || value[length - 1] == '\t' ||
         value[length - 1] == '\r' || value[length - 1] == '\n' ||
         value[length - 1] == '\v' || value[length - 1] == '\f')) --length;
  if (length == 4 && memcmp(value, "play", 4) == 0) return true;
  if (length == 4 && memcmp(value, "wait", 4) == 0) {
    *wait = true;
    return true;
  }
  return false;
}
#endif
