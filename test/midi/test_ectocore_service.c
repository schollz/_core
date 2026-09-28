// Run ectocore's production USB service with a backpressured host.
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "visualizer_telemetry.h"

static unsigned usb_tasks, commands, hardware_polls, controls;
static unsigned realtime_packets, sysex_packets;
#if ZV_ENABLED
static unsigned allowance = 100;
static uint32_t now;
static bool connected = true;
#endif
static void *onewiremidi;
static void audio_media_poll(void) {}
static void metadata_deferred_presets(void) {}
static const unsigned total_number_samples=1;
static void sleep_ms(unsigned n) {(void)n;}
#define ZD_CALL(...) ((void)0)
static void tud_task(void) { ++usb_tasks; }
#if ZV_ENABLED
static bool tud_mounted(void) { return connected; }
static uint32_t time_us_32(void) { return now; }
static bool tud_midi_packet_write(const uint8_t packet[4]) {
  if (!allowance) return false;
  --allowance;
  if (packet[0] == 15) ++realtime_packets;
  else ++sysex_packets;
  return true;
}
#endif
static void command(void) {
  ZV_CALL(assert(!zv_tx_pending()));
  ++commands;
  ZV_CALL(zv_command(0x80, 9, 5, 0, now));
}
#define midi_comm_task(...) command()
static void Onewiremidi_receive(void *input) {
  assert(input == onewiremidi);
  ++hardware_polls;
}
static void iteration(void) {
  for (unsigned i = 0; i < 1; ++i) {
#include "ectocore_service.h"
    ++controls;
  }
  assert(usb_tasks == controls && hardware_polls == controls);
}
int main(void) {
  iteration();
  assert(commands == 1);
#if ZV_ENABLED
  assert(sysex_packets > 0 && !zv_tx_pending());
  now = 20000; allowance = 1;
  iteration();
  assert(commands == 2 && zv_tx_pending());
  unsigned before = sysex_packets;
  zv_realtime_enqueue(0xfa); zv_realtime_enqueue(0xf8); zv_realtime_enqueue(0xfc);
  now++; allowance = 1;
  iteration();
  assert(commands == 2 && sysex_packets == before && realtime_packets == 1);
  assert(zv_tx_pending());
  now++; allowance = 100;
  iteration();
  assert(commands == 2 && realtime_packets == 3 && !zv_tx_pending());
  now++; iteration();
  assert(commands == 3);
  // A disconnected host clears queued telemetry and resumes USB commands.
  now = 40000; allowance = 1; iteration();
  assert(zv_tx_pending());
  connected = false; now++; iteration();
  assert(!zv_tx_pending());
  connected = true; allowance = 100; now++; iteration();
  assert(commands == 5 && !zv_tx_pending());
  // The stalled-host cutoff also leaves hardware input and controls running.
  now = 60000; allowance = 1; iteration();
  assert(zv_tx_pending());
  unsigned commands_before = commands;
  now += 50000; allowance = 0; iteration();
  assert(commands == commands_before && !zv_tx_pending());
#else
  iteration();
  assert(commands == 2 && sysex_packets == 0 && realtime_packets == 0);
#endif
  puts("ectocore foreground MIDI service: passed");
}
