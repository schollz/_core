#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "onewiremidi.pio.h"
#include "pico/stdlib.h"

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

int main() {
  stdio_init_all();

  sleep_ms(1500);  // Wait for the circuit to settle
  printf("clock freq: %2.3f\n", (float)clock_get_hz(clk_sys));

  const uint PIN_CAPTURE = 18;  // Replace with your desired GPIO pin
  PIO pio = pio0;
  unsigned char sm = 0;
  uint offset = pio_add_program(pio0, &midi_rx_program);
  pio_sm_config c = midi_rx_program_get_default_config(offset);
  sm_config_set_in_pins(&c, PIN_CAPTURE);
  pio_sm_set_consecutive_pindirs(pio, sm, PIN_CAPTURE, 1, true);
  sm_config_set_set_pins(&c, PIN_CAPTURE, 1);
  sm_config_set_in_shift(&c, 0, 0, 0);  // Corrected the shift setup
  pio_sm_init(pio, sm, offset, &c);
  pio_sm_set_clkdiv(pio, sm,
                    (float)clock_get_hz(clk_sys) / 1000000.0f);  // 1 us/cycle
  pio_sm_set_enabled(pio, sm, true);

  uint8_t rbs[3];
  uint8_t rbi = 0;
  while (true) {
    sleep_ms(1);
    if (!pio_sm_is_rx_fifo_empty(pio, sm)) {
      uint8_t value = reverse_uint8_t(pio_sm_get(pio, sm));
      rbs[rbi] = value;
      rbi++;
      if (rbi == 1 && rbs[0] == 0 && (value < 0x80 || value > 0x99)) {
        rbi = 0;
      } else if (rbi == 3) {
        printf("%02X %02X %02X\n", rbs[0], rbs[1], rbs[2]);
        rbi = 0;
      }
    }
  }

  return 0;
}
