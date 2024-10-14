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

#include "fixedpoint.h"
#include "resonantfilter_data.h"

#define FILTER_LOWPASS 0
#define FILTER_BANDPASS 1
#define FILTER_HIGHPASS 2
#define FILTER_NOTCH 3

typedef struct ResonantFilter {
  bool passthrough;
  int16_t passthrough_last;
  uint8_t filter_type;
  uint8_t fc;
  uint8_t q;
  int32_t b0;
  int32_t b1;
  int32_t b2;
  int32_t a1;
  int32_t a2;
  int32_t x1_f;
  int32_t x2_f;
  int32_t y1_f;
  int32_t y2_f;
  bool do_setFc;
  uint8_t do_setFc_val;
  bool do_setFilterType;
  uint8_t do_setFilterType_val;
  bool filter_was_on;
} ResonantFilter;

void ResonantFilter_reset(ResonantFilter* rf) {
  if (!rf->passthrough) {
    if (rf->fc >= resonantfilter_fc_max) {
      rf->fc = resonantfilter_fc_max - 1;
    }
    rf->b0 = resonantfilter_data[rf->filter_type][rf->q][rf->fc][0];
    rf->b1 = resonantfilter_data[rf->filter_type][rf->q][rf->fc][1];
    rf->b2 = resonantfilter_data[rf->filter_type][rf->q][rf->fc][2];
    rf->a1 = resonantfilter_data[rf->filter_type][rf->q][rf->fc][3];
    rf->a2 = resonantfilter_data[rf->filter_type][rf->q][rf->fc][4];
  }
}

void ResonantFilter_setFc_(ResonantFilter* rf, uint8_t fc) {
  if (rf->fc == fc) {
    rf->do_setFc = false;
  } else if (rf->fc < fc) {
    rf->fc++;
  } else {
    rf->fc--;
  }
  if (rf->fc >= resonantfilter_fc_max) {
    rf->fc = resonantfilter_fc_max;
    rf->passthrough = true;
  } else {
    rf->passthrough = false;
  }
}

void ResonantFilter_setFc(ResonantFilter* rf, uint8_t fc) {
  rf->do_setFc = true;
  rf->do_setFc_val = fc;
}

void ResonantFilter_setQ(ResonantFilter* rf, uint8_t q) {
  if (q >= resonantfilter_q_max) {
    q = resonantfilter_q_max - 1;
  }
  if (rf->q == q) {
    return;
  }
  rf->q = q;
  ResonantFilter_reset(rf);
}

void ResonantFilter_setFilterType_(ResonantFilter* rf, uint8_t filter_type) {
  if (rf->filter_type == filter_type) {
    return;
  }
  rf->filter_type = filter_type;
  rf->passthrough_last = -1;
}

void ResonantFilter_setFilterType(ResonantFilter* rf, uint8_t filter_type) {
  if (rf->filter_type == filter_type ||
      (rf->do_setFilterType && rf->do_setFilterType_val == filter_type)) {
    return;
  }
  rf->do_setFilterType = true;
  rf->do_setFilterType_val = filter_type;
}

void ResonantFilter_reset2(ResonantFilter* rf, float fc, float fs, float q,
                           float db, uint8_t filter_type) {
  float w0 = 2 * 3.14159265358979732384626 * (fc / fs);
  float cosW = cos(w0);
  float sinW = sin(w0);
  // float A = pow(10, db / 40);
  float alpha = sinW / (2 * q);
  // float beta = pow(A, 0.5) / q;
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

  rf->b0 = q16_16_float_to_fp(b0);
  rf->b1 = q16_16_float_to_fp(b1);
  rf->b2 = q16_16_float_to_fp(b2);
  rf->a1 = q16_16_float_to_fp(a1);
  rf->a2 = q16_16_float_to_fp(a2);
}

ResonantFilter* ResonantFilter_create(uint8_t filter_type) {
  ResonantFilter* rf;
  rf = (ResonantFilter*)malloc(sizeof(ResonantFilter));
  rf->filter_type = 0;
  rf->q = 0;
  rf->fc = resonantfilter_fc_max - 1;
  rf->passthrough = true;
  rf->passthrough_last = 0;
  rf->x1_f = 0;
  rf->x2_f = 0;
  rf->y1_f = 0;
  rf->y2_f = 0;
  rf->do_setFc = false;
  rf->do_setFc_val = 0;
  rf->do_setFilterType = false;
  rf->do_setFilterType_val = 0;
  rf->filter_was_on = false;

  ResonantFilter_reset(rf);
  return rf;
}

#define CROSSFADE_FILTER 8
#define CROSSFADE_FILTER_WAIT 4

void ResonantFilter_update(ResonantFilter* rf, int32_t* samples,
                           uint16_t num_samples, uint8_t channel) {
  if (rf->do_setFilterType && rf->passthrough) {
    ResonantFilter_setFilterType_(rf, rf->do_setFilterType_val);
    rf->do_setFilterType = false;
    if (rf->do_setFc) {
      ResonantFilter_setFc_(rf, rf->do_setFc_val);
    }
    ResonantFilter_reset(rf);
  } else if (rf->do_setFc) {
    ResonantFilter_setFc_(rf, rf->do_setFc_val);
    if (rf->do_setFilterType) {
      ResonantFilter_setFilterType_(rf, rf->do_setFilterType_val);
      rf->do_setFilterType = false;
    }
    ResonantFilter_reset(rf);
  }
  if (rf->passthrough && !rf->filter_was_on) {
    return;
  }
  int32_t y;
  for (uint16_t i = 0; i < num_samples; i++) {
    y = q16_16_multiply(rf->b0, samples[i * 2 + channel]) +
        q16_16_multiply(rf->b1, rf->x1_f) + q16_16_multiply(rf->b2, rf->x2_f) -
        q16_16_multiply(rf->a1, rf->y1_f) - q16_16_multiply(rf->a2, rf->y2_f);

    rf->x2_f = rf->x1_f;
    rf->x1_f = samples[i * 2 + channel];
    rf->y2_f = rf->y1_f;
    rf->y1_f = y;
    if (rf->passthrough && rf->filter_was_on) {
      // fade out the filter
      samples[i * 2 + channel] =
          q16_16_multiply(y, crossfade3_cos_out[i]) +
          q16_16_multiply(samples[i * 2 + channel], crossfade3_cos_in[i]);
    } else if (!rf->passthrough && !rf->filter_was_on) {
      // fade in the filter
      samples[i * 2 + channel] =
          q16_16_multiply(y, crossfade3_cos_in[i]) +
          q16_16_multiply(samples[i * 2 + channel], crossfade3_cos_out[i]);
    } else {
      samples[i * 2 + channel] = y;
    }
  }
  if (rf->passthrough && rf->filter_was_on) {
    rf->filter_was_on = false;
  } else if (!rf->passthrough && !rf->filter_was_on) {
    rf->filter_was_on = true;
  }
}
