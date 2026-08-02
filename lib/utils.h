// Copyright 2023-2025 Zack Scholl, GPLv3.0

#ifndef LIB_UTILS
#define LIB_UTILS 1

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c\n"
#define BYTE_TO_BINARY(byte)                                    \
  ((byte) & 0x80 ? '1' : '0'), ((byte) & 0x40 ? '1' : '0'),     \
      ((byte) & 0x20 ? '1' : '0'), ((byte) & 0x10 ? '1' : '0'), \
      ((byte) & 0x08 ? '1' : '0'), ((byte) & 0x04 ? '1' : '0'), \
      ((byte) & 0x02 ? '1' : '0'), ((byte) & 0x01 ? '1' : '0')

uint16_t bit_set(uint16_t value, uint8_t bit, bool on) {
  if (on) {
    return value | (1 << bit);
  } else {
    return value & ~(1 << bit);
  }
}

// callback definitions
typedef void (*callback_int_int)(int, int);
typedef void (*callback_int)(int);
typedef void (*callback_int32)(int32_t);
typedef void (*callback_uint8_uint8_uint8)(uint8_t, uint8_t, uint8_t);
typedef void (*callback_uint8_uint8)(uint8_t, uint8_t);
typedef void (*callback_uint8)(uint8_t);
typedef void (*callback_uint16)(uint16_t);
typedef void (*callback_void)();

void hue_to_rgb(uint8_t hue, uint8_t *r, uint8_t *g, uint8_t *b) {
  if (hue < 85) {
    *r = hue * 3;
    *g = 255 - hue * 3;
    *b = 0;
  } else if (hue < 170) {
    hue -= 85;
    *r = 255 - hue * 3;
    *g = 0;
    *b = hue * 3;
  } else {
    hue -= 170;
    *r = 0;
    *g = hue * 3;
    *b = 255 - hue * 3;
  }
}

void hue_to_rgb2(uint8_t hue, uint8_t *r, uint8_t *g, uint8_t *b) {
  hue = 255 - hue;
  hue = ((int16_t)hue + 80) % 256;
  if (hue < 85) {
    *r = hue * 3;
    *g = 255 - hue * 3;
    *b = 0;
  } else if (hue < 170) {
    hue -= 85;
    *r = 255 - hue * 3;
    *g = 0;
    *b = hue * 3;
  } else {
    hue -= 170;
    *r = 0;
    *g = hue * 3;
    *b = 255 - hue * 3;
  }
}

#define util_clamp(x, a, b) ((x) > (b) ? (b) : ((x) < (a) ? (a) : (x)))

#define linlin(x, xmin, xmax, ymin, ymax)                                 \
  util_clamp((ymin + (x - xmin) * (ymax - ymin) / (xmax - xmin)), (ymin), \
             (ymax))

static inline char *append_uint32_decimal(char *dest, uint32_t value) {
  char reversed[10];
  uint8_t digits = 0;

  do {
    reversed[digits++] = (char)('0' + value % 10);
    value /= 10;
  } while (value != 0);

  while (digits > 0) {
    *dest++ = reversed[--digits];
  }
  return dest;
}

static inline void format_int32_decimal(char *dest, int32_t value) {
  uint32_t magnitude;
  if (value < 0) {
    *dest++ = '-';
    magnitude = (uint32_t)(-(value + 1)) + 1;
  } else {
    magnitude = (uint32_t)value;
  }
  dest = append_uint32_decimal(dest, magnitude);
  *dest = '\0';
}

static inline void format_prefixed_int32(char *dest, const char *prefix,
                                         int32_t value) {
  while (*prefix != '\0') {
    *dest++ = *prefix++;
  }
  format_int32_decimal(dest, value);
}

static inline void format_sample_filename(char *dest, uint8_t bank,
                                          uint8_t sample, uint8_t variation) {
  memcpy(dest, "bank", 4);
  dest = append_uint32_decimal(dest + 4, (uint32_t)bank + 1);
  *dest++ = '/';
  dest = append_uint32_decimal(dest, sample);
  *dest++ = '.';
  dest = append_uint32_decimal(dest, variation);
  memcpy(dest, ".wav", 5);
}

// multiplies and clips the output
void MultipyAndClip_process(int32_t mul, int16_t max_val, int16_t *values,
                            uint16_t num_values) {
  for (uint16_t i = 0; i < num_values; i++) {
    int32_t v = ((int32_t)values[i] * mul);
    if (v > max_val) {
      v = max_val;
    } else if (v < -max_val) {
      v = -max_val;
    }
    values[i] = v;
  }
}

static inline uint8_t linlin_uint8_t(uint8_t in, uint8_t in_min, uint8_t in_max,
                                     uint8_t out_min, uint8_t out_max) {
  return util_clamp((in - in_min) * (out_max - out_min) / (in_max - in_min) +
                        out_min,
      out_min, out_max);
}

static inline uint16_t linlin_uint16_t(uint8_t in, uint8_t in_min,
                                       uint8_t in_max, uint16_t out_min,
                                       uint16_t out_max) {
  return util_clamp((in - in_min) * (out_max - out_min) / (in_max - in_min) +
                        out_min,
      out_min, out_max);
}

static inline uint32_t linlin_uint32_t(uint8_t in, uint8_t in_min,
                                       uint8_t in_max, uint32_t out_min,
                                       uint32_t out_max) {
  return util_clamp((in - in_min) * (out_max - out_min) / (in_max - in_min) +
                        out_min,
      out_min, out_max);
}

static inline uint8_t linlin_int32_uint8(int32_t in, int32_t in_min,
                                         int32_t in_max, uint8_t out_min,
                                         uint8_t out_max) {
  return util_clamp((in - in_min) * (out_max - out_min) / (in_max - in_min) +
                        out_min,
      out_min, out_max);
}
void generate_euclidean_rhythm(int n, int k, int offset, bool *rhythm) {
  int bucket = 0;

  // Initialize the rhythm array to false
  for (int i = 0; i < n; i++) {
    rhythm[i] = false;
  }

  for (int i = 0; i < n; i++) {
    bucket += k;
    if (bucket >= n) {
      bucket -= n;
      rhythm[i] = true;
    }
  }
  // rotate by offset
  for (int i = 0; i < offset; i++) {
    bool tmp = rhythm[n - 1];
    for (int j = n - 1; j > 0; j--) {
      rhythm[j] = rhythm[j - 1];
    }
    rhythm[0] = tmp;
  }
}
#endif