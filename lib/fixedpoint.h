// Copyright 2023-2024 Zack Scholl.
//
// Author: Zack Scholl (zack.scholl@gmail.com)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// See http://creativecommons.org/licenses/MIT/ for more information.

#ifndef FIXEDPOINT_LIB
#define FIXEDPOINT_LIB 1
/* Defines the number of bits used in the Q16.16 fixed-point format. */
#define Q16_16_Q_BITS 16

// some commons
#define Q16_16_0 0
#define Q16_16_0_5 32768
#define Q16_16_0_75 49152
#define Q16_16_0_9 59882
#define Q16_16_0_85 56555
#define Q16_16_1 65536
#define Q16_16_MINUS_1 -65536
#define Q16_16_2 131072
#define Q16_16_PI 205887
#define Q16_16_2PI 411774
#define Q16_16_PI_OVER_2 102943
#define Q16_16_MAX 2147418112
#define Q16_16_0_333 10923
#define Q16_16_0_398 13015
#define Q16_16_0_050 3277
#define Q16_16_0_035 2294
#define Q16_16_2_OVER_PI 41721  // 2 / pi
// https://www.nullhardware.com/blog/fixed-point-sine-and-cosine-for-embedded-systems/
#define Q16_16_SIN_A5 102873  // 4 * (3/pi - 9/16)
#define Q16_16_SIN_B5 41906   // 2 * a5 - 5 / 2
#define Q16_16_SIN_C5 4569    // a5 - 3/2

/* Defines the number of fractional bits used in the Q16.16 fixed-point format.
 */
#define Q16_16_FRACTIONAL_BITS (1 << Q16_16_Q_BITS)

/* Converts a Q16.16 fixed-point value to an int16. */
int16_t q16_16_fp_to_int16(int32_t fixedValue) {
  /* Shift the fixed-point value right by the number of Q-bits to obtain the
     integral part of the value. */
  return (int16_t)(fixedValue >> Q16_16_Q_BITS);
}

/* Converts a Q16.16 fixed-point value to an int32. */
int32_t q16_16_fp_to_int32(int32_t fixedValue) { return fixedValue; }

/* Converts a Q16.16 fixed-point value to a float. */
float q16_16_fp_to_float(int32_t value) {
  /* Divide the fixed-point value by the number of fractional bits to obtain
     the floating-point value. */
  return (float)value / Q16_16_FRACTIONAL_BITS;
}

/* Converts a float to a Q16.16 fixed-point value. */
int32_t q16_16_float_to_fp(float value) {
  /* Multiply the floating-point value by the number of fractional bits to
     obtain the fixed-point value. */
  return (int32_t)(value * Q16_16_FRACTIONAL_BITS);
}

/* Converts an int16 to a Q16.16 fixed-point value. */
int32_t q16_16_int16_to_fp(int16_t value) {
  /* Shift the int16 value left by the number of Q-bits to obtain the
     fixed-point value. */
  return (int32_t)value << Q16_16_Q_BITS;
}

/* Converts an int32 to a Q16.16 fixed-point value. */
int32_t q16_16_int32_to_fp(int32_t value) {
  /* Shift the int32 value left by the number of Q-bits to obtain the
     fixed-point value. */
  return (int32_t)value << Q16_16_Q_BITS;
}

/* Multiplies two Q16.16 fixed-point values. */
int32_t q16_16_multiply(int32_t a, int32_t b) {
  return (int32_t)((((int64_t)a) * b) >> Q16_16_Q_BITS);
}

int32_t q16_16_divide(int32_t a, int32_t b) {
  /* Divide the two fixed-point values */
  return (int32_t)(((int64_t)a << Q16_16_Q_BITS) / b);
}

int32_t q16_16_sin(int32_t fixedValue) {
  uint8_t negative = 0;
  while (fixedValue < Q16_16_0) {
    fixedValue += Q16_16_PI;
    negative = 1 - negative;
  }
  while (fixedValue > Q16_16_PI) {
    fixedValue -= Q16_16_PI;
    negative = 1 - negative;
  }
  if (fixedValue > Q16_16_PI_OVER_2) {
    fixedValue = Q16_16_PI_OVER_2 - (fixedValue - Q16_16_PI_OVER_2);
  }
  int32_t z = q16_16_multiply(Q16_16_2_OVER_PI, fixedValue);
  int32_t sin5 = q16_16_multiply(Q16_16_SIN_A5, z);
  sin5 -=
      q16_16_multiply(Q16_16_SIN_B5, q16_16_multiply(z, q16_16_multiply(z, z)));
  sin5 += q16_16_multiply(
      Q16_16_SIN_C5,
      q16_16_multiply(
          z, q16_16_multiply(z, q16_16_multiply(z, q16_16_multiply(z, z)))));
  if (negative) {
    return -sin5;
  }
  return sin5;
}

int32_t q16_16_sin01(int32_t fixedValue) {
  int32_t sin5 = q16_16_sin(fixedValue);
  return q16_16_multiply(Q16_16_0_5, sin5) + Q16_16_0_5;
}

int32_t q16_16_cos(int32_t fixedValue) {
  return q16_16_multiply(Q16_16_MINUS_1,
                         q16_16_sin(fixedValue - Q16_16_PI_OVER_2));
}

#endif