// Independent, test-build-only timing outside the diagnostic apparatus.
// Used in BOTH instrumentation-enabled and compiled-out overhead builds.
#ifndef SEEK_TIMING_WITNESS_H
#define SEEK_TIMING_WITNESS_H
#if defined(SEEK_TIMING_WITNESS) && SEEK_TIMING_WITNESS
#include <stdint.h>
#include "pico/time.h"
#include "hardware/sync.h"
typedef struct {
  uint32_t sequence, count, sum_lo, sum_hi, max_us;
  uint32_t active_count, active_sum_lo, active_sum_hi, active_max_us;
} zd_witness_t;
extern volatile zd_witness_t zeptocore_timing_witness;
extern volatile uint32_t zeptocore_witness_rendered;
static inline void zd_witness_record(uint32_t start) {
  // Timestamp before doing any witness bookkeeping; includes all diag hooks.
  uint32_t us = time_us_32() - start;
  volatile zd_witness_t *w = &zeptocore_timing_witness;
  ++w->sequence;
  __dmb();
  ++w->count;
  uint32_t old = w->sum_lo;
  w->sum_lo = old + us;
  if (w->sum_lo < old) ++w->sum_hi;
  if (us > w->max_us) w->max_us = us;
  if (zeptocore_witness_rendered) {
    ++w->active_count;
    old = w->active_sum_lo;
    w->active_sum_lo = old + us;
    if (w->active_sum_lo < old) ++w->active_sum_hi;
    if (us > w->active_max_us) w->active_max_us = us;
  }
  __dmb();
  ++w->sequence;
}
#define ZD_WITNESS_START() uint32_t zd_witness_start = time_us_32(); zeptocore_witness_rendered = 0
#define ZD_WITNESS_END() zd_witness_record(zd_witness_start)
#define ZD_WITNESS_RENDERED() (zeptocore_witness_rendered = 1)
#else
#define ZD_WITNESS_START() ((void)0)
#define ZD_WITNESS_END() ((void)0)
#define ZD_WITNESS_RENDERED() ((void)0)
#endif
#endif
