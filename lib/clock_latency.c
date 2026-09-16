#include "clock_latency.h"
#if SEEK_CLOCK_LATENCY
#include "seek_diagnostics.h"
#include <stdatomic.h>
#include <string.h>
volatile clock_latency_record clock_latency_records[8];
static clock_latency_record current;
static _Atomic uint32_t active, phase_token, applied_token, submitted;
static uint32_t serial;
static void publish(void) {
  volatile clock_latency_record *out = &clock_latency_records[current.id & 7];
  uint32_t seq = out->sequence;
  out->sequence = seq + 1;
  atomic_thread_fence(memory_order_release);
  const uint32_t *src = (const uint32_t *)&current;
  volatile uint32_t *dst = (volatile uint32_t *)out;
  for (unsigned i = 1; i < sizeof(current)/4; ++i) dst[i] = src[i];
  atomic_thread_fence(memory_order_release);
  out->sequence = seq + 2;
  atomic_store_explicit(&active, 0, memory_order_release);
}
void cl_timeout(void) {
  if (atomic_load(&active)) { current.flags |= 1u << 31; publish(); }
}
void cl_begin(uint32_t flags, uint32_t source, uint32_t mode) {
  cl_timeout();
  memset(&current, 0, sizeof current);
  current.id = ++serial;
  current.input_us = time_us_32();
  current.flags = flags;
  current.source = source;
  current.mode = mode;
  current.first_nonzero = UINT32_MAX;
  current.starvation_before = cl_starvation();
  atomic_store(&phase_token, 0); atomic_store(&applied_token, 0);
  atomic_store(&submitted, 0);
  atomic_store_explicit(&active, current.id, memory_order_release);
}
void cl_handler_done(void) { current.handler_done_us = time_us_32(); }
void cl_phase(uint32_t beat, uint32_t phase) {
  uint32_t id = atomic_load_explicit(&active, memory_order_acquire);
  if (!id || atomic_load(&phase_token)) return;
  current.phase_us = time_us_32(); current.phase = phase; current.beat = beat;
  atomic_store_explicit(&phase_token, id, memory_order_release);
}
void cl_applied(uint32_t phase) {
  uint32_t id = atomic_load_explicit(&phase_token, memory_order_acquire);
  if (!id || id != atomic_load(&active) || atomic_load(&applied_token)) return;
  if (phase != current.phase) { current.flags |= 1u << 30; return; }
  current.applied_us = time_us_32();
  atomic_store_explicit(&applied_token, id, memory_order_release);
}
uint32_t cl_render_tag(void) {
  uint32_t id = atomic_load_explicit(&applied_token, memory_order_acquire);
  return id && id == atomic_load(&active) ? CL_TAG | (id << 9) : 0;
}
void cl_seek(int32_t offset, uint32_t frames, uint32_t bytes_per_frame, uint32_t flags) {
  if (cl_render_tag() && !atomic_load(&submitted)) {
    current.negative_latency = offset;
    current.source_frames = frames;
    current.bytes_per_frame = bytes_per_frame;
    current.flags |= flags;
  }
}
void cl_submit(uint32_t tag, const int16_t *pcm, uint32_t frames) {
  uint32_t id = (tag & ~CL_TAG) >> 9;
  if (!(tag & CL_TAG) || id != atomic_load(&active) || atomic_load(&submitted)) return;
  for (unsigned i = 0; i < frames; ++i) {
    if (pcm[2*i] > 4 || pcm[2*i] < -4 || pcm[2*i+1] > 4 || pcm[2*i+1] < -4) {
      current.first_nonzero = i; break;
    }
  }
  current.submit_us = time_us_32();
  atomic_store_explicit(&submitted, id, memory_order_release);
}
uint32_t cl_copy(uint32_t destination, uint32_t source, uint32_t offset) {
  uint32_t id = atomic_load(&active);
  if (id && ((source & ~CL_TAG) >> 9) == id &&
      ((destination & ~CL_TAG) >> 9) != id && offset < 512)
    return CL_TAG | (id << 9) | offset;
  return destination;
}
void cl_dma(uint32_t tag, uint32_t us) {
  uint32_t id = (tag & ~CL_TAG) >> 9;
  if (!id || id != atomic_load_explicit(&submitted, memory_order_acquire) ||
      id != atomic_load(&active)) return;
  current.dma_us = us; current.dma_offset = tag & 511;
  current.starvation_after = cl_starvation();
  publish();
}
#endif
