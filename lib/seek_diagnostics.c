// Copyright 2026 Zack Scholl, GPLv3.0
#include "seek_diagnostics.h"
#include "audio_profile.h"
#include <limits.h>
#include <string.h>

#if defined(SEEK_DIAGNOSTICS) && SEEK_DIAGNOSTICS
#define ZD_RAM __attribute__((section(".time_critical.seek_diagnostics")))
#else
#define ZD_RAM
#endif

const uint32_t zd_bin_upper_us[ZD_BINS] = {
    4, 16, 64, 128, 256, 512, 1024, 2048,
    2903, 4096, 5805, 8192, 10000, 20000, 100000, UINT32_MAX};

static void sat_add(uint32_t *value, uint32_t add, uint32_t *saturated) {
  if (UINT32_MAX - *value < add) {
    *value = UINT32_MAX;
    *saturated = 1;
  } else {
    *value += add;
  }
}
static void wide_add(uint32_t *lo, uint32_t *hi, uint32_t add,
                      uint32_t *saturated) {
  uint32_t next = *lo + add;
  if (next < *lo) {
    if (*hi == UINT32_MAX) {
      *lo = UINT32_MAX;
      *saturated = 1;
      return;
    }
    ++*hi;
  }
  *lo = next;
}
ZD_RAM void zd_metric_record(zd_metric_t *m, uint32_t us, uint32_t error,
                      uint32_t bytes, bool short_read, uint32_t *sat) {
  sat_add(&m->count, 1, sat);
  wide_add(&m->total_lo, &m->total_hi, us, sat);
  if (us > m->max_us) m->max_us = us;
  if (error) sat_add(&m->errors, 1, sat);
  if (short_read) sat_add(&m->short_reads, 1, sat);
  wide_add(&m->bytes_lo, &m->bytes_hi, bytes, sat);
  // Four fixed comparisons, independent of the latency magnitude.
  unsigned lo = 0, hi = ZD_BINS - 1;
  while (lo < hi) {
    unsigned mid = (lo + hi) / 2;
    if (us <= zd_bin_upper_us[mid]) hi = mid;
    else lo = mid + 1;
  }
  sat_add(&m->bins[lo], 1, sat);
}

#if defined(SEEK_DIAGNOSTICS) && SEEK_DIAGNOSTICS
#include "hardware/structs/rosc.h"
#include "hardware/sync.h"
#include "pico/platform.h"
#include "seek_build_identity.h"
#include <stdatomic.h>
#if defined(SEEK_TEST_CONTROLS) && SEEK_TEST_CONTROLS
#define ZD_BASE_FEATURES 517u // diagnostics, musical test controls, protected preset tests
#else
#define ZD_BASE_FEATURES 1u
#endif
#if defined(SEEK_MAP_MODULE) && SEEK_MAP_MODULE
#define ZD_FEATURES (ZD_BASE_FEATURES | 8u | 32u | 64u | 128u | (SEEK_MAP_ATTACH ? 16u : 0u) | (SEEK_TEST_FRESH_INDEX ? 256u : 0u) | (SEEK_TEST_AUDIO_FIXTURE ? 1024u : 0u))
#else
#define ZD_FEATURES ZD_BASE_FEATURES
#endif
__attribute__((weak)) void zd_application_control_snapshot(void) {}
__attribute__((weak)) void zd_application_layout_snapshot(volatile uint32_t *out,uint16_t id) {
  for(unsigned i=0;i<76;++i)out[i]=0;
  (void)id;
}

volatile zd_mailbox_t zeptocore_diag = {.header = {
    ZD_MAGIC, ZD_ABI, sizeof(zd_mailbox_t), 128, ZD_FEATURES, 0, 44100,
    SAMPLES_PER_BUFFER, ZD_BUILD_WORDS
    0, 0, 0, 0, 0, 0, 0, 0,
    offsetof(zd_mailbox_t, request), offsetof(zd_mailbox_t, audio),
    offsetof(zd_mailbox_t, control), offsetof(zd_mailbox_t, irq),
    sizeof(zd_audio_t), sizeof(zd_control_t), sizeof(zd_irq_t), 0}};
zd_audio_t zd_audio;
zd_control_t zd_control;
static zd_irq_t zd_irq;
static uint32_t audio_start_us, last_dma_us;
static bool dma_started;
static _Atomic uint32_t switch_requested, switch_completed, switch_key, switch_start;
static _Atomic uint32_t switch_file_token, switch_rejected, switch_expired;
static uint32_t switch_sequence;
_Static_assert(sizeof(zd_mailbox_t) + sizeof(zd_audio_t) +
                   sizeof(zd_control_t) + sizeof(zd_irq_t) + 44 <= 4096,
               "diagnostics exceed 4 KiB static budget");

void zd_init(uint32_t clock_hz, uint32_t frames) {
  // Fresh boot token from the free-running ring oscillator, no flash writes.
  uint32_t token = 0;
  for (unsigned i = 0; i < 32; ++i)
    token = (token << 1) ^ rosc_hw->randombit;
  zeptocore_diag.header[5] = clock_hz;
  zeptocore_diag.header[7] = frames;
  zeptocore_diag.header[16] = token;
  zeptocore_diag.header[17] = time_us_32();
  zeptocore_diag.header[21] = 1;
#if defined(__arm__)
  extern uint32_t __scratch_y_end__, __StackTop;
  uintptr_t sp;
  __asm volatile ("mov %0, sp" : "=r" (sp));
  // Paint only the unallocated part BELOW this function's current stack frame.
  // The guard is extra conservatism, not a claim of measured high-water usage.
  // The linker reserves a minimum 2 KiB, but the stack may use the whole
  // otherwise unoccupied scratch-Y bank. Never paint actual scratch data.
  uintptr_t bottom = ((uintptr_t)&__scratch_y_end__ + 3u) & ~(uintptr_t)3u;
  uintptr_t end = (sp - 64u) & ~(uintptr_t)3u;
  zd_control.context[16] = bottom;
  zd_control.context[17] = end;
  zd_control.context[18] = (uintptr_t)&__StackTop;
  if (bottom < end && end <= (uintptr_t)&__StackTop) {
    for (volatile uint32_t *p = (volatile uint32_t *)bottom;
         (uintptr_t)p < end; ++p) *p = 0xa55aa55au;
  }
#endif
  __dmb();
}

void zd_prepare_core1_stack(void) {
#if defined(__arm__)
  extern uint32_t __StackOneBottom, __StackOneTop;
  // Called after resetting core 1 and BEFORE launching it onto this stack.
  zd_control.context[19] = (uintptr_t)&__StackOneBottom;
  zd_control.context[20] = (uintptr_t)&__StackOneTop;
  for (volatile uint32_t *p = &__StackOneBottom; p < &__StackOneTop; ++p)
    *p = 0xa55aa55au;
  __dmb();
#endif
}

ZD_RAM void zd_service(unsigned domain) {
  volatile zd_response_header_t *h;
  volatile uint32_t *out;
  const uint32_t *in;
  uint32_t bytes, progress;
  if (domain == ZD_AUDIO) {
    h = &zeptocore_diag.audio.header;
    out = (volatile uint32_t *)&zeptocore_diag.audio.data;
    in = (const uint32_t *)&zd_audio;
    bytes = sizeof(zd_audio);
  } else if (domain == ZD_CONTROL) {
    h = &zeptocore_diag.control.header;
    out = (volatile uint32_t *)&zeptocore_diag.control.data;
    in = (const uint32_t *)&zd_control;
    bytes = sizeof(zd_control);
  } else if (domain == ZD_IRQ) {
    h = &zeptocore_diag.irq.header;
    out = (volatile uint32_t *)&zeptocore_diag.irq.data;
    in = (const uint32_t *)&zd_irq;
    bytes = sizeof(zd_irq);
  } else return;
  progress = ++zeptocore_diag.header[18 + domain]; // modular progress only
  uint32_t seq = zeptocore_diag.request.sequence;
  if (!seq || seq == h->sequence) return;
  __dmb();
  uint32_t command = zeptocore_diag.request.command;
  uint32_t argument=zeptocore_diag.request.arguments[0];
  bool malformed = command==ZD_MAP_LAYOUT && argument>UINT16_MAX;
  for (unsigned i = 0; i < 6; ++i)
    if(i || command!=ZD_MAP_LAYOUT)malformed |= zeptocore_diag.request.arguments[i] != 0;
  __dmb();
  if (seq != zeptocore_diag.request.sequence) return;
  uint32_t start = time_us_32();
  h->sequence = 0; // invalidate before touching response payload
  __dmb();
  uint32_t status = malformed ? ZD_MALFORMED :
      command == ZD_SNAPSHOT || command == ZD_MAP_LAYOUT ? ZD_OK : ZD_UNSUPPORTED;
  if (status == ZD_OK) {
    if(domain==ZD_CONTROL && command==ZD_SNAPSHOT)zd_application_control_snapshot();
    if(domain==ZD_IRQ) {
      zd_irq.counters[12]=atomic_load_explicit(&switch_requested,memory_order_acquire);
      zd_irq.counters[13]=atomic_load_explicit(&switch_completed,memory_order_relaxed);
      zd_irq.counters[14]=atomic_load_explicit(&switch_rejected,memory_order_relaxed);
      zd_irq.counters[15]=atomic_load_explicit(&switch_expired,memory_order_relaxed);
    }
    // All domain payloads are multiples of 16 bytes. Four words per loop keeps
    // publication within its budget on the supported 125 MHz clock as well.
    if(domain==ZD_CONTROL && command==ZD_MAP_LAYOUT) {
      for(unsigned i=76;i<bytes/4;i+=4)out[i]=out[i+1]=out[i+2]=out[i+3]=0;
      zd_application_layout_snapshot(out,(uint16_t)argument);
    } else {
      for (unsigned i = 0; i < bytes / 4; i += 4) {
        out[i] = in[i];
        out[i + 1] = in[i + 1];
        out[i + 2] = in[i + 2];
        out[i + 3] = in[i + 3];
      }
    }
  }
  h->status = status;
  h->timestamp_us = start;
  h->progress = progress;
  h->payload_bytes = status == ZD_OK ? bytes : 0;
  h->generation = seq;
  h->publish_us = time_us_32() - start;
  if (domain == ZD_AUDIO && h->publish_us > zd_audio.counters[ZD_PUBLISH_MAX_US])
    zd_audio.counters[ZD_PUBLISH_MAX_US] = h->publish_us;
  if (domain == ZD_CONTROL && h->publish_us > zd_control.counters[10])
    zd_control.counters[10] = h->publish_us;
  if (domain == ZD_IRQ && h->publish_us > zd_irq.counters[8])
    zd_irq.counters[8] = h->publish_us;
  __dmb();
  h->sequence = seq;
}
void zd_audio_counter(unsigned c) {
  if (c < 16) sat_add(&zd_audio.counters[c], 1,
                     &zd_audio.counters[ZD_SATURATED]);
}
void zd_audio_begin(void) { audio_start_us = time_us_32(); }
ZD_RAM void zd_audio_end(void) {
  zd_service(ZD_AUDIO);
  uint32_t us = time_us_32() - audio_start_us;
  zd_metric_record(&zd_audio.metrics[ZD_CALLBACK], us, 0, 0, false,
                   &zd_audio.counters[ZD_SATURATED]);
  // Actual I2S period may differ from the nominal 44.1 kHz.
  uint32_t budget = zeptocore_diag.header[31];
  if (!budget) budget = SAMPLES_PER_BUFFER * 1000000u / 44100u;
  if (us > budget) zd_audio_counter(ZD_CALLBACK_OVERRUN);
}
ZD_RAM void zd_audio_output(uint32_t frames, unsigned disposition) {
  sat_add(&zd_audio.counters[ZD_FRAMES], frames,
          &zd_audio.counters[ZD_SATURATED]);
  if (disposition == 1) zd_audio_counter(ZD_MUTED);
  if (disposition == 2) zd_audio_counter(ZD_ERROR_SILENCE);
}
ZD_RAM void zd_record_io(unsigned kind, uint32_t start, uint32_t result,
                  uint32_t bytes, bool short_read) {
  if (kind >= ZD_AUDIO_METRICS) return;
  zd_metric_record(&zd_audio.metrics[kind], time_us_32() - start,
                   result, bytes, short_read, &zd_audio.counters[ZD_SATURATED]);
  if (result) zd_audio.counters[ZD_LAST_ERROR] = result;
}
ZD_RAM void zd_record_seek(unsigned kind, uint32_t start, uint32_t result,
                    uint64_t from, uint64_t requested, uint64_t position,
                    bool mapped) {
  uint32_t us = time_us_32() - start;
  zd_metric_record(&zd_audio.metrics[kind], us, result, 0, false,
                   &zd_audio.counters[ZD_SATURATED]);
  zd_audio_counter(requested > from ? ZD_SEEK_FORWARD :
                   requested < from ? ZD_SEEK_BACKWARD : ZD_SEEK_SAME);
  if (result) zd_audio.counters[ZD_LAST_ERROR] = result;
  if (us >= zd_audio.worst_seek[0]) {
    uint32_t *w = zd_audio.worst_seek;
    w[0] = us; w[1] = from; w[2] = from >> 32;
    w[3] = requested; w[4] = requested >> 32;
    w[5] = position; w[6] = position >> 32;
    w[7] = result; w[8] = kind;
    w[9] = zd_audio.counters[ZD_FILE_GENERATION];
    w[10] = start; w[11] = mapped;
  }
}
ZD_RAM void zd_dma_start(bool missing, uint32_t frames) {
  uint32_t now = time_us_32();
  if (dma_started)
    zd_metric_record(&zd_irq.metrics[ZD_DMA_INTERVAL], now - last_dma_us,
                     0, frames, false, &zd_irq.counters[7]);
  last_dma_us = now;
  dma_started = true;
  sat_add(&zd_irq.counters[0], 1, &zd_irq.counters[7]);
  if (missing) {
    AP_CALL(if(zeptocore_diag.header[21]>=3)audio_profile_starved(now));
    unsigned base = zeptocore_diag.header[21] >= 3 ? 1 : 3;
    sat_add(&zd_irq.counters[base], 1, &zd_irq.counters[7]);
    sat_add(&zd_irq.counters[base + 1], frames, &zd_irq.counters[7]);
  }
}
ZD_RAM void zd_notify(uint32_t start, bool success) {
  zd_metric_record(&zd_irq.metrics[ZD_NOTIFY_WAIT], time_us_32() - start,
                   !success, 0, false, &zd_irq.counters[7]);
  if (success) zd_irq.counters[6] = 0;
  else {
    sat_add(&zd_irq.counters[5], 1, &zd_irq.counters[7]);
    sat_add(&zd_irq.counters[6], 1, &zd_irq.counters[7]);
  }
  zd_service(ZD_IRQ);
}
void zd_switch_request(uint16_t key) {
  // The MIDI control dispatcher on core 0 is the only request writer. DMA on
  // that core can interrupt it, so publish the token after all request fields.
  uint32_t now=time_us_32(), pending=atomic_load(&switch_requested);
  if(pending && pending!=atomic_load(&switch_completed)) {
    if(now-atomic_load(&switch_start)<2000000u) {
      switch_rejected=switch_rejected+1;return;
    }
    switch_expired=switch_expired+1;
  }
  atomic_store(&switch_requested,0);
  switch_sequence=(switch_sequence+1)&0x7fffffu;if(!switch_sequence)switch_sequence=1;
  atomic_store_explicit(&switch_key,key,memory_order_relaxed);
  atomic_store_explicit(&switch_start,now,memory_order_relaxed);
  atomic_store_explicit(&switch_requested,switch_sequence,memory_order_release);
}
void zd_switch_file(uint32_t key) {
  // Called by the exclusive filesystem owner after a successful open, or with
  // UINT32_MAX on close/failure. A conflicting publication simply skips tracing.
  uint32_t seq=atomic_load_explicit(&switch_requested,memory_order_acquire);
  bool matches=seq && key==atomic_load_explicit(&switch_key,memory_order_relaxed);
  if(seq!=atomic_load_explicit(&switch_requested,memory_order_acquire))matches=false;
  atomic_store_explicit(&switch_file_token,matches?seq:0,memory_order_release);
}
uint32_t zd_switch_render_tag(void) {
  return atomic_load_explicit(&switch_file_token,memory_order_acquire)<<9;
}
uint32_t zd_switch_copy_tag(uint32_t destination,uint32_t source,uint32_t offset) {
  uint32_t seq=atomic_load_explicit(&switch_requested,memory_order_acquire);
  // Preserve the first occurrence when a producer is split or several producers
  // are combined. Completed/old tokens cannot hide the next measured transition.
  if(seq && (source>>9)==seq && (destination>>9)!=seq && offset<512)
    return (seq<<9)|offset;
  return destination;
}
void zd_switch_dma(uint32_t tag,uint32_t started_us) {
  uint32_t seq=tag>>9;
  if(!seq || seq!=atomic_load_explicit(&switch_requested,memory_order_acquire) ||
     seq==atomic_load_explicit(&switch_completed,memory_order_relaxed))return;
  uint32_t elapsed=started_us-atomic_load_explicit(&switch_start,memory_order_relaxed);
  uint32_t rate=zeptocore_diag.header[22];
  if(!rate || elapsed>=2000000u)return;
  elapsed+=(uint64_t)(tag&511u)*1000000000u/rate;
  sat_add(&zd_irq.counters[9],1,&zd_irq.counters[7]);
  zd_irq.counters[10]=elapsed;
  if(elapsed>zd_irq.counters[11])zd_irq.counters[11]=elapsed;
  atomic_store_explicit(&switch_completed,seq,memory_order_release);
}
void zd_audio_clock(uint32_t clock_hz, uint32_t divider, uint32_t bits) {
  zeptocore_diag.header[22] =
      (uint32_t)((uint64_t)clock_hz * 1000 / (divider * bits * 4));
  zeptocore_diag.header[23] = divider;
  zeptocore_diag.header[31] = (uint32_t)(
      (uint64_t)SAMPLES_PER_BUFFER * 1000000000u / zeptocore_diag.header[22]);
}
#endif
