#ifndef LIB_BITCRUSH_H_
#define LIB_BITCRUSH_H_

void Bitcrush_process(int16_t *values, uint16_t num_values) {
  for (int i = 0; i < num_values; i++) {
    // reduce sample rate
    // TODO: make this a parameter
    if (i % 5 == 0) {
      // reduce bit rate
      // TODO: make this a parameter
      values[i] = (values[i] >> 8) << 8;
    } else {
      values[i] = values[i - 1];
    }
  }
}

#endif