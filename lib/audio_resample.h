// Copyright 2026 Zack Scholl, GPLv3.0
#ifndef AUDIO_RESAMPLE_H
#define AUDIO_RESAMPLE_H
#include <stdint.h>

// Reserve the interpolation lookahead frame in the fixed source workspace.
static inline uint32_t audio_source_frame_limit(uint32_t requested,
                                               uint32_t capacity_samples,
                                               uint32_t channels) {
  uint32_t capacity = capacity_samples / channels - 1;
  return requested < capacity ? requested : capacity;
}

// Identical 9-bit interpolation arithmetic for contiguous or interleaved input.
// The caller supplies arr_size frames plus one lookahead frame.
static inline void audio_resample_linear(int16_t *out, const int16_t *source,
                                         uint32_t arr_size, uint32_t out_size,
                                         uint32_t stride) {
  uint32_t step = arr_size * 512u / out_size;
  for (uint32_t i = 0; i < out_size; ++i) {
    uint32_t position = i * step;
    uint32_t index = position / 512u, frac = position % 512u;
    int32_t value = (int32_t)source[index * stride] * (512u - frac) +
                    (int32_t)source[(index + 1) * stride] * frac;
    out[i] = value / 512;
  }
}
#endif
