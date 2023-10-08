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

  const uint PIN_CAPTURE = 22;  // Replace with your desired GPIO pin

  gpio_init(PIN_CAPTURE);
  gpio_set_dir(PIN_CAPTURE, GPIO_IN);

  sleep_ms(1500);  // Wait for the circuit to settle

  printf("waiting for midi data\n");
  while (true) {
    // // Wait for the input pin to go LOW
    while (!gpio_get(PIN_CAPTURE)) {
      tight_loop_contents();
    }
    sleep_us(16);

    // Read 8 bits at 31,250 baud
    uint8_t received_byte1 = 0;
    for (int i = 0; i < 8; i++) {
      sleep_us(32);  // Wait for 1 bit time at 31,250 baud
      received_byte1 |= (!gpio_get(PIN_CAPTURE) << i);  // Read the bit
    }
    sleep_us(32);  // Wait for 1 bit time at 31,250 baud
    while (!gpio_get(PIN_CAPTURE)) {
      tight_loop_contents();
    }
    sleep_us(16);

    uint8_t received_byte2 = 0;
    for (int i = 0; i < 8; i++) {
      sleep_us(32);  // Wait for 1 bit time at 31,250 baud
      received_byte2 |= (!gpio_get(PIN_CAPTURE) << i);  // Read the bit
    }
    sleep_us(32);
    while (!gpio_get(PIN_CAPTURE)) {
      tight_loop_contents();
    }
    sleep_us(16);

    uint8_t received_byte3 = 0;
    for (int i = 0; i < 8; i++) {
      sleep_us(32);  // Wait for 1 bit time at 31,250 baud
      received_byte3 |= (!gpio_get(PIN_CAPTURE) << i);  // Read the bit
    }
    if (received_byte1 > 0 || received_byte2 > 0 || received_byte3 > 0) {
      printf("\n\n%02X %02X %02X\n", received_byte1, received_byte2,
             received_byte3);
    }
  }

  return 0;
}
