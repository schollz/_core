#ifndef LIB_VU_H
#define LIB_VU_H
#include <stddef.h>
#include <stdint.h>

#define SMOOTHING_FACTOR 230
#define MAX_LEVEL 255
#define FIXED_POINT_SHIFT 15
#define INT32_MAX_VAL 2147483647

uint64_t calculate_squared_sum(const int32_t *buffer, size_t len) {
  uint64_t sum = 0;
  for (size_t i = 0; i < len; i++) {
    int64_t sample = (int64_t)abs(buffer[i]);
    sum += (sample * sample) >> FIXED_POINT_SHIFT;
  }
  return sum;
}

uint8_t map_to_vu_level(uint64_t squared_sum, size_t len) {
  uint64_t mean_squared = squared_sum / len;
  uint32_t rms = 0;
  while (mean_squared > (uint64_t)(rms + 1) * (rms + 1)) {
    rms++;
  }
  return (uint8_t)((rms * MAX_LEVEL) / (INT32_MAX_VAL >> FIXED_POINT_SHIFT));
}

uint8_t vu_meter_readout(const int32_t *buffer, size_t len) {
  static uint8_t previous_level = 0;
  uint64_t squared_sum = calculate_squared_sum(buffer, len);
  uint8_t current_level = map_to_vu_level(squared_sum, len);
  uint8_t smoothed_level = (previous_level * SMOOTHING_FACTOR +
                            current_level * (256 - SMOOTHING_FACTOR)) >>
                           8;
  previous_level = smoothed_level;
  return smoothed_level;
}
#endif
