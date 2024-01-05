#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "hardware/spi.h"
#include "pico/stdlib.h"
//
#include "../../mcp3208.h"

int main(void) {
  stdio_init_all();

  MCP3208 *mcp3208 = MCP3208_malloc(spi1, 9, 10, 8, 11);
  while (1) {
    sleep_ms(10);
    for (uint8_t channel = 0; channel < 8; channel++) {
      printf("%d ", MCP3208_read(mcp3208, channel, false));
    }
    printf("\n");
  }

  // // Pins
  // const uint cs_pin = 9;
  // const uint sck_pin = 10;
  // const uint mosi_pin = 8;   // rx
  // const uint miso_pin = 11;  // tx

  // // Initialize CS pin high
  // gpio_init(cs_pin);
  // gpio_set_dir(cs_pin, GPIO_OUT);
  // gpio_put(cs_pin, 1);

  // // Initialize SPI port at 1 MHz
  // spi_init(spi1, 1000 * 1000);

  // // Initialize SPI pins
  // gpio_set_function(sck_pin, GPIO_FUNC_SPI);
  // gpio_set_function(mosi_pin, GPIO_FUNC_SPI);
  // gpio_set_function(miso_pin, GPIO_FUNC_SPI);

  // while (1) {
  //   sleep_ms(10);
  //   for (uint8_t channel = 0; channel < 8; channel++) {
  //     uint8_t buffer[3];
  //     uint8_t data[3];
  //     bool differential = false;
  //     buffer[0] = 0x01;
  //     buffer[1] = ((differential ? 0 : 1) << 7) | (channel << 4);
  //     buffer[3] = 0x00;
  //     data[0] = 0x00;
  //     data[1] = 0x00;
  //     data[2] = 0x00;
  //     gpio_put(cs_pin, 0);
  //     int num_bytes_wrote = spi_write_read_blocking(spi1, &buffer, &data, 3);
  //     gpio_put(cs_pin, 1);
  //     uint16_t num = 1023 - (data[2] + (data[1] << 8));
  //     printf(" %d ", num);
  //   }
  //   printf("\n");
  // }
  return 0;
}
