#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "hardware/spi.h"
#include "pico/stdlib.h"
//

int main(void) {
  stdio_init_all();

  sleep_ms(1000);
  printf("MCP3208 test\n");

  while (1) {
    sleep_ms(10);
  }

  return 0;
}
