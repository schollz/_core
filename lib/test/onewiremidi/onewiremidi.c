#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "pico/stdlib.h"

int main() {
  stdio_init_all();

  const uint PIN_CAPTURE = 18;  // Replace with your desired GPIO pin

  gpio_init(PIN_CAPTURE);
  gpio_set_dir(PIN_CAPTURE, GPIO_IN);

  sleep_ms(1000);  // Wait for the circuit to settle

  while (true) {
    // Wait for the input pin to go LOW
    while (gpio_get(PIN_CAPTURE)) {
      tight_loop_contents();
    }

    // Read 8 bits at 31,250 baud
    uint8_t received_byte = 0;
    for (int i = 0; i < 8; i++) {
      sleep_us(32);  // Wait for 1 bit time at 31,250 baud
      received_byte |= (gpio_get(PIN_CAPTURE) << i);  // Read the bit
    }

    // Print the received byte
    printf("Received byte: 0x%02X\n", received_byte);

    // Wait for the input pin to go HIGH (optional)
    while (!gpio_get(PIN_CAPTURE)) {
      tight_loop_contents();
    }
  }

  return 0;
}
