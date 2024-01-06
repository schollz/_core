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

  return 0;
}
