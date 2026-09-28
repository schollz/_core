#ifndef CV_INPUT_DETECT_H
#define CV_INPUT_DETECT_H
#include <stdbool.h>
#include <stdint.h>

// An empty jack follows the driven test pattern. Derive its threshold from
// this channel's current response, never from other jacks or a saved baseline.
static inline bool cv_input_follows_pattern(const uint16_t *samples,
    const uint8_t *pattern, unsigned count, unsigned margin,
    unsigned allowed_errors, uint16_t *strength) {
  uint32_t sums[2] = {0, 0};
  unsigned counts[2] = {0, 0};
  *strength = 0;
  for (unsigned i = 0; i < count; ++i) {
    unsigned bit = pattern[i] != 0;
    sums[bit] += samples[i];
    ++counts[bit];
  }
  if (!counts[0] || !counts[1]) return false;
  uint32_t low = sums[0] / counts[0], high = sums[1] / counts[1];
  if (high <= low || high - low <= 2 * margin) return false;
  *strength = (high - low) / 2;
  uint32_t midpoint = (low + high) / 2;
  unsigned errors = 0;
  for (unsigned i = 0; i < count; ++i) {
    bool matches = pattern[i] ? samples[i] > midpoint + margin
                              : samples[i] + margin < midpoint;
    if (!matches && ++errors > allowed_errors) return false;
  }
  return true;
}
#endif
