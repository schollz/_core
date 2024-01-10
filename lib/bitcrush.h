#ifndef LIB_BITCRUSH_H_
#define LIB_BITCRUSH_H_

void Bitcrush_process(int16_t *values, uint16_t num_values, uint8_t sample_rate,
                      uint8_t bitrate) {
  sample_rate = linlin(sample_rate, 0, 255, 1, 8);
  bitrate = linlin(bitrate, 0, 255, 0, 12);
  for (int i = 0; i < num_values; i++) {
    // reduce sample rate
    // TODO: make this a parameter
    if (i % sample_rate == 0) {
      // reduce bit rate
      // TODO: make this a parameter
      values[i] = (values[i] >> bitrate) << bitrate;
    } else {
      values[i] = values[i - 1];
    }
  }
}

#endif