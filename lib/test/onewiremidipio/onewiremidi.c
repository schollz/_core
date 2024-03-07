
#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "pico/stdlib.h"
//
#include "../../filterexp.h"
#include "../../onewiremidi.h"

void printBinaryRepresentation(uint8_t num) {
  // Iterate through each bit position (from 31 to 0)
  for (int i = 8; i >= 0; i--) {
    // Extract the i-th bit using bitwise operations
    uint8_t bit = (num >> i) & 1;
    printf("%u", bit);  // Print the bit
  }
  printf("\n");
}

uint8_t reverse_uint8_t(uint8_t b) {
  b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
  b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
  b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
  return b;
}

void midi_note_on(uint8_t note, uint8_t vel) {
  printf("note on\t\t%d\t%d\n", note, vel);
}

void midi_note_off(uint8_t note) { printf("note off\t%d\n", note); }

void midi_start() { printf("midi start\n"); }

void midi_continue() { printf("midi continue\n"); }

void midi_stop() { printf("midi stop\n"); }

uint32_t midi_last_time = 0;
uint32_t midi_delta_sum = 0;
uint32_t midi_delta_count = 0;
#define MIDI_DELTA_COUNT_MAX 32
void midi_timing() {
  // printf("midi timing\n");
  uint32_t now_time = time_us_32();
  if (midi_last_time > 0) {
    midi_delta_sum += now_time - midi_last_time;
    midi_delta_count++;
    if (midi_delta_count == MIDI_DELTA_COUNT_MAX) {
      printf("midi bpm\t%d\n", (int)round(2500000.0 * MIDI_DELTA_COUNT_MAX /
                                          (float)(midi_delta_sum)));
      midi_delta_count = 0;
      midi_delta_sum = 0;
    }
  }
  midi_last_time = now_time;
}

int main() {
  stdio_init_all();

  sleep_ms(4500);  // Wait for the circuit to settle
  printf("clock freq: %2.3f\n", (float)clock_get_hz(clk_sys));

  Onewiremidi *om;
  om = Onewiremidi_new(pio0, 0, 22, midi_note_on, midi_note_off, midi_start,
                       midi_continue, midi_stop, midi_timing);
  while (true) {
    Onewiremidi_receive(om);
    // sleep_ms(2);
  }

  return 0;
}
