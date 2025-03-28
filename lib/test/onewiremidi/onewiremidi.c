#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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

int main() {
  stdio_init_all();

  const uint PIN_CAPTURE = 18;  // Replace with your desired GPIO pin

  gpio_init(PIN_CAPTURE);
  gpio_set_dir(PIN_CAPTURE, GPIO_IN);
  gpio_pull_up(PIN_CAPTURE);

  sleep_ms(1500);  // Wait for the circuit to settle

  printf("waiting for midi data\n");
  uint8_t received_byte[3];
  uint8_t received_i = 0;
  uint8_t received_c = 0;
  while (true) {
    // wait for input pin to go low
    while (gpio_get(PIN_CAPTURE)) {
      tight_loop_contents();
    }
    // sleep for 16 microseconds
    sleep_us(16);

    // read in 8 bits into a byte
    uint8_t rb = 0;
    for (int i = 0; i < 8; i++) {
      // for each bit, wait 32 microseconds
      sleep_us(32);
      // read in bit
      rb |= (gpio_get(PIN_CAPTURE) << i);  // Read the bit
    }
    // sleep 32 microseconds
    sleep_us(32);

    received_byte[received_i] = rb;
    received_i++;
    if (received_i == 1) {
      if (received_byte[0] < 0x80 || received_byte[0] > 0x9F) {
        received_i = 0;
      }
    }
    if (received_i == 3) {
      printf("\n\n%02X %02X %02X\n", received_byte[0], received_byte[1],
             received_byte[2]);
      received_i = 0;
    }
  }

  return 0;
}
