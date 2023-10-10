
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
  printf("note on\t%d\t%d", note, vel);
}

void midi_note_off(uint8_t note) { printf("note off\t%d", note); }

void midi_start() { printf("midi start\n"); }

void midi_continue() { printf("midi continue\n"); }

void midi_stop() { printf("midi stop\n"); }

int main() {
  stdio_init_all();

  sleep_ms(1500);  // Wait for the circuit to settle
  printf("clock freq: %2.3f\n", (float)clock_get_hz(clk_sys));

  Onewiremidi *om;
  om = Onewiremidi_new(pio0, 0, 18, midi_note_on, midi_note_off, midi_start,
                       midi_continue, midi_stop, NULL);

  while (true) {
    Onewiremidi_receive(om);
  }

  return 0;
}
