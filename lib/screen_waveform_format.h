// Copyright 2026 Zack Scholl, GPLv3.0
#ifndef SCREEN_WAVEFORM_FORMAT_H
#define SCREEN_WAVEFORM_FORMAT_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// Explicit wire offsets, shared by the JUCE writer and RP2040 reader.
enum { SW_COLUMNS = 384, SW_HEADER_BYTES = 64, SW_FILE_BYTES = 448 };
static inline uint16_t sw_u16(const uint8_t *p) {
  return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}
static inline uint32_t sw_u32(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
         ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static inline void sw_put16(uint8_t *p, uint16_t v) {
  p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
}
static inline void sw_put32(uint8_t *p, uint32_t v) {
  for (unsigned n = 0; n < 4; ++n) p[n] = (uint8_t)(v >> (8 * n));
}
static inline uint32_t sw_crc32(const uint8_t *p, size_t n) {
  uint32_t crc = UINT32_MAX;
  while (n--) {
    crc ^= *p++;
    for (unsigned b = 0; b < 8; ++b)
      crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
  }
  return ~crc;
}
static inline uint32_t sw_info_hash(const uint8_t *p, size_t n) {
  uint32_t h = 2166136261u;
  while (n--) { h ^= *p++; h *= 16777619u; }
  return h;
}
static inline bool sw_valid(const uint8_t *p, size_t n, uint32_t pcm_bytes,
                            uint32_t rate, unsigned channels, uint32_t info_hash) {
  return n == SW_FILE_BYTES && !memcmp(p, "ZCW1", 4) &&
      sw_u16(p + 4) == 1 && sw_u16(p + 6) == SW_COLUMNS &&
      pcm_bytes && channels >= 1 && channels <= 2 &&
      (rate == 44100 || rate == 88200) && pcm_bytes % (channels * 2) == 0 &&
      sw_u32(p + 8) == pcm_bytes && sw_u32(p + 12) == rate &&
      p[16] == channels && p[17] == 1 && sw_u16(p + 18) == 0 &&
      sw_u32(p + 20) == info_hash &&
      sw_u32(p + 56) == sw_crc32(p + SW_HEADER_BYTES, SW_COLUMNS) &&
      sw_u32(p + 60) == sw_crc32(p, 60);
}
#endif
