// Include the production serial decoder below, with only its hardware byte source stubbed.
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "midi_receive.h"
enum { MIDI_NOTE_OFF = 0x80, MIDI_NOTE_ON = 0x90, MIDI_CONTROL_CHANGE = 0xb0,
       MIDI_TIMING_CLOCK = 0xf8, MIDI_START = 0xfa, MIDI_CONTINUE = 0xfb, MIDI_STOP = 0xfc };
typedef struct {
  int pio, sm;
  uint8_t status, previous;
  uint32_t last_time;
  void (*midi_note_on)(int, int);
  void (*midi_note_off)(int);
  void (*midi_start)(void), (*midi_continue)(void), (*midi_stop)(void), (*midi_timing)(void);
  void (*midi_control_change)(uint8_t, uint8_t, uint8_t);
} Onewiremidi;
static uint8_t byte;
static uint32_t now;
static uint32_t time_us_32(void) { return now += 320; }
static bool pio_sm_is_rx_fifo_empty(int pio, int sm) { (void)pio; (void)sm; return false; }
static uint8_t pio_sm_get(int pio, int sm) { (void)pio; (void)sm; return byte; }
// The physical input is inverted/reversed; this stub presents its decoded value.
static uint8_t Onewiremidi_reverse_uint8_t(uint8_t b) { return (uint8_t)~b; }
#include "serial_decoder.h"
static unsigned ons, offs, ccs, clocks, starts, stops, continues;
static void on(int n, int v) { assert(n == 60 && v == 100); ++ons; }
static void off(int n) { assert(n == 60); ++offs; }
static void cc(uint8_t ch, uint8_t n, uint8_t v) { assert(ch == midi_receive_channel - 1 && n == 7 && v == 64); ++ccs; }
static void clock_tick(void) { ++clocks; }
static void start(void) { ++starts; }
static void stop(void) { ++stops; }
static void resume(void) { ++continues; }
static void feed(Onewiremidi *m, uint8_t b) { byte = b; Onewiremidi_receive_(m); }
int main(void) {
  Onewiremidi m = {.midi_note_on=on, .midi_note_off=off, .midi_control_change=cc,
    .midi_timing=clock_tick, .midi_start=start, .midi_stop=stop, .midi_continue=resume};
  for (unsigned selected = 1; selected <= 16; ++selected) {
    midi_receive_channel = selected;
    for (unsigned channel = 0; channel < 16; ++channel) {
      unsigned a=ons, b=offs, c=ccs, clock_before=clocks;
      feed(&m, 0x90 | channel); feed(&m, 60); feed(&m, 0xf8); feed(&m, 100);
      // Running status retains the full channel across interleaved realtime.
      feed(&m, 60); feed(&m, 100); feed(&m, 60); feed(&m, 0);
      feed(&m, 0x80 | channel); feed(&m, 60); feed(&m, 1);
      feed(&m, 0xb0 | channel); feed(&m, 7); feed(&m, 64);
      unsigned match = channel == selected - 1;
      assert(ons == a + 2*match && offs == b + 2*match && ccs == c + match);
      assert(clocks == clock_before + 1);
    }
    feed(&m, 0xfa); feed(&m, 0xfc); feed(&m, 0xfb);
  }
  assert(starts == 16 && stops == 16 && continues == 16);
  puts("Serial MIDI channels, running status, CCs and realtime passed");
}
