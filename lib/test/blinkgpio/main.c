#include <stdio.h>
#include <stdlib.h>

#include "hardware/pio.h"
#include "pico/stdlib.h"

#define SN_RCLK 5
#define SN_SRCLK 6
#define SN_SER 7
#define SN_DELAY 1

// set_led_bits drives the 8-bit shift register
// 1000000X
// -------1
void set_led_bits(uint8_t leds) {
  uint8_t row = 0;
  gpio_put(SN_SER, row);
  sleep_us(SN_DELAY);
  gpio_put(SN_SRCLK, 1);
  sleep_us(SN_DELAY);
  gpio_put(SN_SRCLK, 0);
  sleep_us(SN_DELAY);
  for (uint8_t i = 0; i < 7; i++) {
    gpio_put(SN_SER, (leds >> i) & 1);
    sleep_us(SN_DELAY);
    gpio_put(SN_SRCLK, 1);
    sleep_us(SN_DELAY);
    gpio_put(SN_SRCLK, 0);
    sleep_us(SN_DELAY);
  }
  gpio_put(SN_RCLK, 1);
  sleep_us(SN_DELAY);
  gpio_put(SN_RCLK, 0);
  sleep_us(SN_DELAY);
}

void set_led_bits2(uint16_t leds) {
  // top row
  gpio_put(SN_SER, 0);
  sleep_us(SN_DELAY);
  gpio_put(SN_SRCLK, 1);
  sleep_us(SN_DELAY);
  gpio_put(SN_SRCLK, 0);
  sleep_us(SN_DELAY);
  for (uint8_t i = 0; i < 7; i++) {
    gpio_put(SN_SER, (leds >> i) & 1);
    sleep_us(SN_DELAY);
    gpio_put(SN_SRCLK, 1);
    sleep_us(SN_DELAY);
    gpio_put(SN_SRCLK, 0);
    sleep_us(SN_DELAY);
  }
  gpio_put(SN_RCLK, 1);
  sleep_us(SN_DELAY);
  gpio_put(SN_RCLK, 0);
  sleep_us(SN_DELAY);
  sleep_us(1);

  // bottom row
  gpio_put(SN_SER, 1);
  sleep_us(SN_DELAY);
  gpio_put(SN_SRCLK, 1);
  sleep_us(SN_DELAY);
  gpio_put(SN_SRCLK, 0);
  sleep_us(SN_DELAY);
  for (uint8_t i = 7; i < 14; i++) {
    gpio_put(SN_SER, (leds >> i) & 1);
    sleep_us(SN_DELAY);
    gpio_put(SN_SRCLK, 1);
    sleep_us(SN_DELAY);
    gpio_put(SN_SRCLK, 0);
    sleep_us(SN_DELAY);
  }
  gpio_put(SN_RCLK, 1);
  sleep_us(SN_DELAY);
  gpio_put(SN_RCLK, 0);
  sleep_us(SN_DELAY);
  sleep_us(1);
}

int main(void) {
  stdio_init_all();

  gpio_init(SN_RCLK);
  gpio_set_dir(SN_RCLK, GPIO_OUT);
  gpio_init(SN_SRCLK);
  gpio_set_dir(SN_SRCLK, GPIO_OUT);
  gpio_init(SN_SER);
  gpio_set_dir(SN_SER, GPIO_OUT);

  // test code to see if leds work
  while (true) {
    uint16_t x = 0;
    // set pin 3 and pin 11
    x |= (1 << 3);
    x |= (1 << 11);
    for (uint8_t i = 0; i < 14; i++) {
      set_led_bits2(x);
    }
  }

  while (true) {
    printf("on\n");
    for (uint8_t i = 0; i < 8; i++) {
      gpio_put(SN_SER, 1);
      gpio_put(SN_SRCLK, 1);
      gpio_put(SN_SRCLK, 0);
    }
    gpio_put(SN_RCLK, 1);
    gpio_put(SN_RCLK, 0);
    sleep_ms(100);
    printf("oFF\n");
    for (uint8_t i = 0; i < 8; i++) {
      gpio_put(SN_SER, 0);
      gpio_put(SN_SRCLK, 1);
      gpio_put(SN_SRCLK, 0);
    }
    gpio_put(SN_RCLK, 1);
    gpio_put(SN_RCLK, 0);
    sleep_ms(100);
  }

  // for (uint8_t i = 12; i < 16; i++) {
  //   gpio_init(i);
  //   gpio_set_dir(i, GPIO_OUT);
  // }

  // while (1) {
  //   printf("low\n");
  //   for (uint8_t i = 12; i < 16; i++) {
  //     gpio_put(i, 0);
  //   }
  //   sleep_ms(1000);
  //   printf("high\n");
  //   for (uint8_t i = 12; i < 16; i++) {
  //     gpio_put(i, 1);
  //   }
  //   sleep_ms(1000);
  // }
  return 0;
}
