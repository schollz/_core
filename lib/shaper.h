#ifndef LIB_SHAPER_LIB
#define LIB_SHAPER_LIB 1
#include "shapers.h"

void Shaper_expandOver_compressUnder_process(int16_t *values,
                                             uint16_t num_values,
                                             int16_t threshold) {
  for (uint16_t i = 0; i < num_values; i++) {
    if (abs(values[i]) > threshold) {
      if (values[i] < 0) {
        // expand
        values[i] = -1 * expand_curve_data[abs(values[i]) >> SHAPER_REDUCE];
      } else {
        // expand
        values[i] = expand_curve_data[abs(values[i]) >> SHAPER_REDUCE];
      }
    } else {
      // compress
      if (values[i] < 0) {
        values[i] = -1 * compress_curve_data[abs(values[i]) >> SHAPER_REDUCE];
      } else {
        values[i] = compress_curve_data[abs(values[i]) >> SHAPER_REDUCE];
      }
    }
  }
}

void Shaper_expandUnder_compressOver_process(int16_t *values,
                                             uint16_t num_values,
                                             int16_t threshold) {
  for (uint16_t i = 0; i < num_values; i++) {
    if (abs(values[i]) > threshold) {
      // compress
      if (values[i] < 0) {
        values[i] = -1 * compress_curve_data[abs(values[i]) >> SHAPER_REDUCE];
      } else {
        values[i] = compress_curve_data[abs(values[i]) >> SHAPER_REDUCE];
      }
    } else {
      // expand
      if (values[i] < 0) {
        values[i] = -1 * expand_curve_data[abs(values[i]) >> SHAPER_REDUCE];
      } else {
        values[i] = expand_curve_data[abs(values[i]) >> SHAPER_REDUCE];
      }
    }
  }
}

#endif