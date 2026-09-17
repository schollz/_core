// Copyright 2026 Zack Scholl, GPLv3.0
#include "visualizer_telemetry.h"
#if ZV_ENABLED
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#ifndef ZV_HOST_TEST
#include "hardware/sync.h"
#endif

static _Atomic uint32_t trigger_word;
static uint16_t trigger_serial; // core 0, protected against its timer IRQ
static _Atomic uint32_t revision, published_state, published_trigger;
static bool subscribed, sent_once, requested;
static uint32_t renewed_at, sent_at, pending_at;
static zv_snapshot last_sent;
static uint8_t pending[64];
static unsigned pending_size, pending_offset;
static uint8_t realtime_queue[32];
static _Atomic uint32_t realtime_head, realtime_tail;

void zv_realtime_enqueue(uint8_t status) {
#ifndef ZV_HOST_TEST
  uint32_t irq = save_and_disable_interrupts();
#endif
  uint32_t head = atomic_load_explicit(&realtime_head, memory_order_relaxed);
  uint32_t tail = atomic_load_explicit(&realtime_tail, memory_order_acquire);
  // Bounded like the USB FIFO: don't stall the musical timer for a slow host.
  if ((uint32_t)(head - tail) < sizeof realtime_queue) {
    realtime_queue[head % sizeof realtime_queue] = status;
    atomic_store_explicit(&realtime_head, head + 1, memory_order_release);
  }
#ifndef ZV_HOST_TEST
  restore_interrupts(irq);
#endif
}
void zv_realtime_service(bool connected, bool (*write_packet)(const uint8_t[4])) {
  uint32_t tail = atomic_load_explicit(&realtime_tail, memory_order_relaxed);
  uint32_t head = atomic_load_explicit(&realtime_head, memory_order_acquire);
  if (!connected) {
    atomic_store_explicit(&realtime_tail, head, memory_order_release);
    return;
  }
  for (unsigned i = 0; i < 8 && tail != head; ++i) {
    uint8_t packet[4] = {0x0f, realtime_queue[tail % sizeof realtime_queue], 0, 0};
    if (!write_packet(packet)) return;
    atomic_store_explicit(&realtime_tail, ++tail, memory_order_release);
  }
}

void zv_trigger(uint8_t bank, uint8_t sample, uint8_t slice) {
#ifndef ZV_HOST_TEST
  uint32_t irq = save_and_disable_interrupts();
#endif
  uint32_t word = ((uint32_t)++trigger_serial << 16) |
      ((uint32_t)(bank & 15) << 12) | ((uint32_t)(sample & 15) << 8) | slice;
  atomic_store_explicit(&trigger_word, word, memory_order_release);
#ifndef ZV_HOST_TEST
  restore_interrupts(irq);
#endif
}
uint32_t zv_trigger_read(void) {
  return atomic_load_explicit(&trigger_word, memory_order_acquire);
}

static uint32_t pack(const zv_snapshot *s) {
  return (s->bank & 15u) | ((s->sample & 15u) << 4) |
      ((uint32_t)s->slice << 8) | ((uint32_t)s->forward << 16) |
      ((uint32_t)s->stopped << 17) | ((uint32_t)s->muted << 18) |
      ((uint32_t)s->valid << 19) | ((s->bpm & 511u) << 20);
}
void zv_publish(const zv_snapshot *s) {
  // One writer, aligned atomic loads/stores only. Sequential consistency
  // prevents a reader accepting fields from two different render callbacks.
  uint32_t serial = atomic_load(&revision);
  atomic_store(&revision, serial + 1);
  atomic_store(&published_state, pack(s));
  atomic_store(&published_trigger, s->trigger | ((uint32_t)s->effects << 16));
  atomic_store(&revision, serial + 2);
}
bool zv_read(zv_snapshot *s) {
  uint32_t before = atomic_load(&revision);
  if (before & 1) return false;
  uint32_t word = atomic_load(&published_state);
  uint32_t trigger = atomic_load(&published_trigger);
  if (before != atomic_load(&revision)) return false;
  *s = (zv_snapshot){.bank = word & 15, .sample = (word >> 4) & 15,
      .slice = (word >> 8) & 255, .trigger = trigger & 65535,
      .effects = trigger >> 16, .bpm = (word >> 20) & 511,
      .forward = (word >> 16) & 1, .stopped = (word >> 17) & 1,
      .muted = (word >> 18) & 1, .valid = (word >> 19) & 1};
  return true;
}
bool zv_command(uint8_t status, uint8_t channel, uint8_t note,
                uint8_t velocity, uint32_t now) {
  if (status != 0x80 || channel != 9 || note != 5 || velocity != 0) return false;
  if (!subscribed || (uint32_t)(now - renewed_at) >= 2000000) sent_once = false;
  subscribed = true;
  requested = true;
  renewed_at = now;
  return true;
}
bool zv_tx_pending(void) { return pending_size != 0; }
static bool same(const zv_snapshot *a, const zv_snapshot *b) {
  return pack(a) == pack(b) && a->trigger == b->trigger && a->effects == b->effects;
}
void zv_service(uint32_t now, bool connected, bool (*write_packet)(const uint8_t[4])) {
  if (!connected) {
    subscribed = sent_once = requested = false;
    pending_size = pending_offset = 0;
    return;
  }
  if ((uint32_t)(now - renewed_at) >= 2000000) subscribed = false;
  // A stalled host must not hold foreground controls hostage. A subsequent F0
  // starts a fresh message; consumers never accept the abandoned partial frame.
  if (pending_size && (uint32_t)(now - pending_at) >= 50000) {
    pending_size = pending_offset = 0;
    subscribed = sent_once = requested = false;
    return;
  }
  if (!pending_size) {
    if (!subscribed || (sent_once && (uint32_t)(now - sent_at) < 16667)) return;
    zv_snapshot snapshot;
    if (!zv_read(&snapshot)) return;
    if (sent_once && !requested && same(&snapshot, &last_sent) && (uint32_t)(now - sent_at) < 250000) return;
    // Version 2 adds the effect bitmask. Transmission remains bounded to
    // 16 USB packets per service call; longer frames resume next time.
    pending[0] = 0xf0;
    int n = snprintf((char *)pending + 1, sizeof pending - 2,
        "view=2,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u", snapshot.bank,
        snapshot.sample, snapshot.slice, snapshot.trigger, snapshot.bpm,
        snapshot.forward, snapshot.stopped, snapshot.muted, snapshot.valid, snapshot.effects);
    if (n < 0 || (unsigned)n >= sizeof pending - 2) return;
    pending[n + 1] = 0xf7;
    pending_size = n + 2;
    pending_offset = 0;
    pending_at = now;
    last_sent = snapshot;
  }
  for (unsigned i = 0; i < 16 && pending_size; ++i) {
    unsigned remaining = pending_size - pending_offset;
    unsigned count = remaining < 3 ? remaining : 3;
    uint8_t packet[4] = {remaining <= 3 ? (uint8_t)(4 + remaining) : 4, 0, 0, 0};
    memcpy(packet + 1, pending + pending_offset, count);
    if (!write_packet(packet)) return;
    pending_offset += count;
    if (pending_offset == pending_size) {
      pending_size = pending_offset = 0;
      sent_at = now;
      sent_once = true;
      requested = false;
    }
  }
}
#endif
