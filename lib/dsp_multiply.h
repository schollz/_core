#ifndef DSP_MULTIPLY_H
#define DSP_MULTIPLY_H
#include <stdint.h>

// Low 32 bits of the signed 64-bit product shifted right by 16, exactly.
// a = ah*65536 + al, b = bh*65536 + bl. Therefore the result is
// ah*b + al*bh + ((al*bl)>>16), modulo 2^32. Unsigned arithmetic keeps
// overflow defined and lets the Cortex-M0+ use three 32-bit multiplications.
static inline __attribute__((always_inline)) int32_t dsp_multiply_q16(
    int32_t a, int32_t b) {
  const uint32_t al = (uint16_t)a, bl = (uint16_t)b;
  return (int32_t)((uint32_t)(a >> 16) * (uint32_t)b +
                   al * (uint32_t)(b >> 16) + ((al * bl) >> 16));
}
#endif
