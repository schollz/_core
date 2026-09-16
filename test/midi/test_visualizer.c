// Production telemetry tested with a bounded, backpressured USB packet sink.
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include "visualizer_telemetry.h"

static uint8_t bytes[512];
static unsigned length, packets, allowance = 100;
static bool sink(const uint8_t packet[4]) {
  if (!allowance) return false;
  --allowance; ++packets;
  unsigned cin = packet[0], n = cin == 15 ? 1 : cin == 4 ? 3 : cin - 4;
  assert(((cin >= 4 && cin <= 7) || cin == 15) && length + n < sizeof bytes);
  memcpy(bytes + length, packet + 1, n); length += n;
  return true;
}
static void clear(void) { length = packets = 0; allowance = 100; memset(bytes, 0, sizeof bytes); }
static atomic_bool done;
static void *publisher(void *unused) {
  (void)unused;
  for (unsigned i = 0; i < 50000; ++i) {
    zv_snapshot s = {.bank = i & 15, .sample = i & 15, .slice = i & 15,
        .trigger = i & 15, .bpm = i & 15, .valid = true};
    zv_publish(&s);
  }
  atomic_store(&done, true); return NULL;
}
int main(void) {
  zv_realtime_enqueue(0xfa); zv_realtime_enqueue(0xf8); zv_realtime_enqueue(0xfc);
  assert(packets == 0);
  allowance = 1; zv_realtime_service(true, sink); assert(length == 1 && bytes[0] == 0xfa);
  allowance = 100; zv_realtime_service(true, sink);
  assert(length == 3 && bytes[1] == 0xf8 && bytes[2] == 0xfc);
  clear();
  for (unsigned i = 0; i < 1000; i++) zv_realtime_enqueue(0xf8);
  zv_realtime_service(true, sink); assert(packets == 8);
  zv_realtime_service(false, sink); clear(); zv_realtime_service(true, sink); assert(packets == 0);
  zv_snapshot s = {.bank = 1, .sample = 2, .slice = 3, .trigger = 4,
      .bpm = 170, .forward = true, .valid = true, .effects = 32769};
  zv_publish(&s);
  zv_service(0, true, sink); assert(packets == 0);
  assert(!zv_command(0x80, 9, 5, 1, 0)); // Never treat a performance key as subscription.
  assert(!zv_command(0x80, 0, 5, 0, 0));
  assert(zv_command(0x80, 9, 5, 0, 0));
  zv_service(0, true, sink);
  assert(bytes[0] == 0xf0 && bytes[length - 1] == 0xf7 && packets <= 16);
  const char expected[] = "view=2,1,2,3,4,170,1,0,0,1,32769";
  assert(length == sizeof expected + 1);
  assert(memcmp(bytes + 1, expected, sizeof expected - 1) == 0);
  clear(); s.trigger++; zv_publish(&s);
  zv_service(16666, true, sink); assert(packets == 0);
  allowance = 2; zv_service(16667, true, sink); assert(zv_tx_pending() && packets == 2);
  allowance = 100; zv_service(16668, true, sink); assert(!zv_tx_pending() && bytes[length - 1] == 0xf7);
  clear(); zv_service(250000, true, sink); assert(packets == 0);
  zv_service(266668, true, sink); assert(packets > 0); // heartbeat
  clear(); zv_service(2000000, true, sink); assert(packets == 0); // lease expired
  zv_command(0x80, 9, 5, 0, UINT32_MAX - 100);
  zv_service(UINT32_MAX - 100, true, sink); assert(packets > 0);
  clear(); zv_service(300000, true, sink); assert(packets > 0); // clock wrap
  zv_service(300001, false, sink); clear(); zv_service(300002, true, sink); assert(packets == 0);
  zv_command(0x80, 9, 5, 0, 400000); allowance = 1;
  zv_service(400000, true, sink); assert(zv_tx_pending());
  allowance = 0; zv_service(450000, true, sink); assert(!zv_tx_pending());
  s.valid = false; s.stopped = true; s.muted = true; zv_publish(&s);
  zv_snapshot read; assert(zv_read(&read) && !read.valid && read.stopped && read.muted);
  zv_trigger(1, 2, 3); uint32_t first = zv_trigger_read();
  zv_trigger(1, 2, 3); assert((uint16_t)first == (uint16_t)zv_trigger_read());
  assert((first >> 16) + 1 == zv_trigger_read() >> 16);
  // Concurrent publication cannot produce a mixture of two callbacks.
  s = (zv_snapshot){0}; zv_publish(&s);
  pthread_t thread; assert(!pthread_create(&thread, NULL, publisher, NULL));
  do {
    if (zv_read(&read)) assert(read.bank == read.sample && read.bank == read.slice && read.bank == read.trigger && read.bank == read.bpm);
  } while (!atomic_load(&done));
  assert(!pthread_join(thread, NULL));
  puts("visualizer telemetry: passed");
}
