
#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "hardware/pio.h"
#include "pico/stdlib.h"
#include "uart_rx.pio.h"

int main() {
  stdio_init_all();

  sleep_ms(500);  // Wait for the circuit to settle
  printf("clock freq: %2.3f\n", (float)clock_get_hz(clk_sys));
  // Set up the state machine we're going to use to receive them.
  PIO pio = pio0;
  uint sm = 0;
  uint offset = pio_add_program(pio, &uart_rx_program);
  uart_rx_program_init(pio, sm, offset, 15, 31250);
  // Echo characters received from PIO to the console
  while (true) {
    char s[36];
    memset(s, 0, sizeof(s));
    for (uint i = 0; i < 6; i++) {
      if (!pio_sm_is_rx_fifo_empty(pio, sm)) {
        char c = uart_rx_program_getc(pio, sm);
        if (c != 0xf8) {
          sprintf(s, "%s%02x ", s, c);
        }
      }
      sleep_us(16);
    }
    if (s[0] != 0) {
      printf("%s\n", s);
    }
    sleep_ms(1);
  }

  return 0;
}
