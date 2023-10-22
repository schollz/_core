
#define Q16_16_Q_BITS 16
#define Q16_16_FRACTIONAL_BITS (1 << Q16_16_Q_BITS)

int16_t q16_16_fp_to_int16(int32_t fixedValue) {
  return (int16_t)(fixedValue >> Q16_16_Q_BITS);
}

float q16_16_fp_to_float(int32_t value) {
  return (float)value / Q16_16_FRACTIONAL_BITS;
}

int32_t q16_16_float_to_fp(float value) {
  return (int32_t)(value * Q16_16_FRACTIONAL_BITS);
}

int32_t q16_16_int16_to_fp(int16_t value) {
  return (int32_t)value << Q16_16_Q_BITS;
}

int32_t q16_16_multiply(int32_t a, int32_t b) {
  return (int32_t)(((int64_t)a * b) >> Q16_16_Q_BITS);
}
