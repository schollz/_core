#define FILTER_LOWPASS 0
#define FILTER_BANDPASS 1
#define FILTER_HIGHPASS 2
#define FILTER_NOTCH 3

#define RESONANT_FILTER_Q_BITS 16
#define RESONANT_FILTER_FRACTIONAL_BITS (1 << RESONANT_FILTER_Q_BITS)
#define PI_FLOAT 3.14159265358979

// Function to convert int16_t to Q8.10 fixed-point
int32_t int16ToFixedPoint(int16_t value) {
  return (int32_t)value << RESONANT_FILTER_Q_BITS;
}

int32_t floatToFixedPoint(float value) {
  return (int32_t)(value * RESONANT_FILTER_FRACTIONAL_BITS);
}
// Function to convert Q8.10 fixed-point to int16_t
int16_t fixedPointToInt16(int32_t fixedValue) {
  return (int16_t)(fixedValue >> RESONANT_FILTER_Q_BITS);
}

// Function to convert a Q8.10 fixed-point value to a float
float fixedPointToFloat(int32_t value) {
  return (float)value / RESONANT_FILTER_FRACTIONAL_BITS;
}

int32_t multiplyFixedPoint(int32_t a, int32_t b) {
  return (int32_t)(((int64_t)a * b) >> RESONANT_FILTER_Q_BITS);
}

// Function to divide two Q8.10 fixed-point numbers
int32_t divideFixedPoint(int32_t numerator, int32_t denominator) {
  // Ensure that the denominator is not zero to avoid division by zero
  if (denominator == 0) {
    // Handle division by zero
    // You can choose to return an error code or a special value
    return INT32_MAX;  // For example, return the maximum value
  }

  // Perform fixed-point division
  int32_t result =
      (int32_t)(((int64_t)numerator << RESONANT_FILTER_Q_BITS) / denominator);
  return result;
}

typedef struct ResonantFilter {
  int32_t b0;
  int32_t b1;
  int32_t b2;
  int32_t a1;
  int32_t a2;
  int32_t x1_f;
  int32_t x2_f;
  int32_t y1_f;
  int32_t y2_f;
} ResonantFilter;

void ResonantFilter_copy(ResonantFilter* rf1, ResonantFilter* rf2) {
  rf2->b0 = rf1->b0;
  rf2->b1 = rf1->b1;
  rf2->b2 = rf1->b2;
  rf2->a1 = rf1->a1;
  rf2->a2 = rf1->a2;
  rf2->x1_f = rf1->x1_f;
  rf2->x2_f = rf1->x2_f;
  rf2->y1_f = rf1->y1_f;
  rf2->y2_f = rf1->y2_f;
}

void ResonantFilter_reset(ResonantFilter* rf, float fc, float fs, float q,
                          float db, uint8_t filter_type) {
  float w0 = 2 * PI_FLOAT * (fc / fs);
  float cosW = cos(w0);
  float sinW = sin(w0);
  float A = pow(10, db / 40);
  float alpha = sinW / (2 * q);
  float beta = pow(A, 0.5) / q;
  float b0, b1, b2, a0, a1, a2;

  if (filter_type == FILTER_HIGHPASS) {
    b0 = (1 + cosW) / 2;
    b1 = -(1 + cosW);
    b2 = b0;
    a0 = 1 + alpha;
    a1 = -2 * cosW;
    a2 = 1 - alpha;
  } else {
    // Low pass
    b1 = 1 - cosW;
    b0 = b1 / 2;
    b2 = b0;
    a0 = 1 + alpha;
    a1 = -2 * cosW;
    a2 = 1 - alpha;
  }

  b0 = b0 / a0;
  b1 = b1 / a0;
  b2 = b2 / a0;
  a1 = a1 / a0;
  a2 = a2 / a0;

  rf->b0 = floatToFixedPoint(b0);
  rf->b1 = floatToFixedPoint(b1);
  rf->b2 = floatToFixedPoint(b2);
  rf->a1 = floatToFixedPoint(a1);
  rf->a2 = floatToFixedPoint(a2);
}

ResonantFilter* ResonantFilter_create(float fc, float fs, float q, float db,
                                      uint8_t filter_type) {
  ResonantFilter* rf;
  rf = (ResonantFilter*)malloc(sizeof(ResonantFilter));
  ResonantFilter_reset(rf, fc, fs, q, db, filter_type);
  return rf;
}

int16_t ResonantFilter_update(ResonantFilter* rf, int16_t in) {
  int32_t x = int16ToFixedPoint(in);
  int32_t y = multiplyFixedPoint(rf->b0, x) +
              multiplyFixedPoint(rf->b1, rf->x1_f) +
              multiplyFixedPoint(rf->b2, rf->x2_f) -
              multiplyFixedPoint(rf->a1, rf->y1_f) -
              multiplyFixedPoint(rf->a2, rf->y2_f);

  rf->x2_f = rf->x1_f;
  rf->x1_f = x;
  rf->y2_f = rf->y1_f;
  rf->y1_f = y;
  return fixedPointToInt16(y);
}
