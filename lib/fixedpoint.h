// Copyright 2023 Zack Scholl.
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
#define Q16_16_2 131072
#define Q16_16_0_5 32768

/* Defines the number of fractional bits used in the Q16.16 fixed-point format.
 */
#define Q16_16_FRACTIONAL_BITS (1 << Q16_16_Q_BITS)

/* Converts a Q16.16 fixed-point value to an int16. */
int16_t q16_16_fp_to_int16(int32_t fixedValue) {
  /* Shift the fixed-point value right by the number of Q-bits to obtain the
     integral part of the value. */
  return (int16_t)(fixedValue >> Q16_16_Q_BITS);
}

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

/* Multiplies two Q16.16 fixed-point values. */
int32_t q16_16_multiply(int32_t a, int32_t b) {
  /* Multiply the two fixed-point values and shift the result right by the
     number of Q-bits to obtain the product. */
  return (int32_t)(((int64_t)a * b) >> Q16_16_Q_BITS);
}

#endif