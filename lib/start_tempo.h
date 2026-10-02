#ifndef CORE_START_TEMPO_H
#define CORE_START_TEMPO_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// Zero disables the startup override; it is never a playing tempo.
static inline bool start_tempo_valid(uint16_t bpm) {
  return bpm == 0 || (bpm >= 30 && bpm <= 300);
}

// Canonical decimal BPM or "default", with optional trailing whitespace.
static inline bool start_tempo_parse(const char *value, size_t length, uint16_t *bpm) {
  *bpm = 0;
  while (length && (value[length - 1] == ' ' || value[length - 1] == '\t' ||
         value[length - 1] == '\r' || value[length - 1] == '\n' ||
         value[length - 1] == '\v' || value[length - 1] == '\f')) --length;
  if (length == 7 && memcmp(value, "default", 7) == 0) return true;
  if (length < 2 || length > 3 || value[0] == '0') return false;
  unsigned n = 0;
  for (size_t i = 0; i < length; ++i) {
    if (value[i] < '0' || value[i] > '9') return false;
    n = n * 10 + (unsigned)(value[i] - '0');
  }
  if (n < 30 || n > 300) return false;
  *bpm = (uint16_t)n;
  return true;
}

static inline uint16_t start_tempo_resolve(uint16_t configured, uint16_t current) {
  return configured && start_tempo_valid(configured) ? configured : current;
}
#endif
