#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define __not_in_flash_func(name) name
#include "crossfade4_441.h"
#include "tapedelay.h"
#include "resonantfilter.h"
#include "reference.h"

static uint32_t rng = 0x812349ab;
static uint32_t random_word(void) {
  rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
  return rng;
}
static int32_t a[882], b[882];
static void inputs(unsigned n) {
  for (unsigned i = 0; i < 2*n; ++i)
    a[i] = b[i] = (int32_t)(random_word() % 200000001) - 100000000;
}
static void check_delay(void) {
  Delay *old = Delay_malloc(), *now = Delay_malloc();
  assert(old && now);
  const float times[] = {0, 0.25f, 1, 100, 9999.75f, 10000, 5000.5f};
  for (unsigned block = 0; block < 4000; ++block) {
    unsigned n = (unsigned[]){0, 1, 64, 128, 256, 441}[block % 6];
    if (block % 17 == 0) {
      Delay_setActive(old, block % 51 != 0);
      Delay_setDuration(old, times[(block / 17) % 7]);
      Delay_setFeedbackf(old, (block % 101) / 101.0f);
    }
    if (block % 29 == 0) old->write_index = old->buffer_size - 1;
    *now = *old;
    inputs(n);
    reference_delay(old, a, n, block % 2);
    Delay_process(now, b, n, block % 2);
    assert(memcmp(a, b, n * 2 * sizeof(*a)) == 0);
    assert(memcmp(old, now, sizeof(*old)) == 0);
  }
  // Exercise tiny rings, every wrap position and full-scale clipping without
  // overflowing the reference interpolation's signed sample subtraction.
  for (unsigned size = 1; size <= 17; ++size) {
    for (unsigned index = 0; index < size; ++index) {
      for (unsigned channel = 0; channel < 2; ++channel) {
        old->buffer_size = size;
        old->write_index = index;
        old->delay_time = old->smoothed_delay_time = (float)size;
        old->on = true;
        Delay_setFeedbackf(old, .99f);
        for (unsigned i = 0; i < size; ++i) old->buffer[i] = 1000000000;
        *now = *old;
        a[0] = b[0] = a[1] = b[1] = INT32_MAX;
        reference_delay(old, a, 1, channel);
        Delay_process(now, b, 1, channel);
        assert(memcmp(a, b, 2 * sizeof(*a)) == 0);
        assert(memcmp(old, now, sizeof(*old)) == 0);
      }
    }
  }
  Delay_process(NULL, b, 256, 0);
  Delay_free(old); Delay_free(now);
}
static void check_filter(void) {
  ResonantFilter old = {0}, now = {0};
  for (unsigned block = 0; block < 12000; ++block) {
    unsigned n = (unsigned[]){0, 1, 64, 128, 256, 441}[block % 6];
    if (block % 7 == 0) {
      ResonantFilter_setFc(&old, random_word() % (resonantfilter_fc_max + 1));
      ResonantFilter_setFilterType(&old, random_word() % 2);
    }
    if (block % 101 == 0) {
      old.passthrough = (block / 101) % 2;
      old.filter_was_on = (block / 202) % 2;
    }
    if (block % 193 == 0) {
      old.fc = (block / 193) % resonantfilter_fc_max;
      old.filter_type = (block / 193) % 2;
      ResonantFilter_reset(&old);
    }
    now = old;
    inputs(n);
    reference_filter(&old, a, n, block % 2);
    ResonantFilter_update(&now, b, n, block % 2);
    assert(memcmp(a, b, n * 2 * sizeof(*a)) == 0);
    assert(memcmp(&old, &now, sizeof(old)) == 0);
  }
}
int main(void) {
  const int32_t edges[] = {INT32_MIN, INT32_MIN+1, -65537, -65536, -1,
                          0, 1, 65535, 65536, INT32_MAX};
  for (unsigned i = 0; i < sizeof edges / sizeof *edges; ++i)
    for (unsigned j = 0; j < sizeof edges / sizeof *edges; ++j)
      assert(dsp_multiply_q16(edges[i], edges[j]) ==
             q16_16_multiply(edges[i], edges[j]));
  for (unsigned i = 0; i < 2000000; ++i) {
    int32_t x = (int32_t)random_word(), y = (int32_t)random_word();
    assert(dsp_multiply_q16(x, y) == q16_16_multiply(x, y));
  }
  check_delay(); check_filter();
  puts("DSP reference equivalence: 4000 delay blocks, 12000 filter blocks passed");
}
