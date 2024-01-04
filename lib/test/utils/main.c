#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "../../utils.h"

int main() {
  uint8_t x = 24;
  printf("map(%d, 0, 255, 0, 100) = %d\n", x,
         linlin_uint8_t(x, 0, 255, 0, 100));
  printf("map(%d, 0, 30, 0, 60000) = %d\n", x,
         linlin_uint16_t(x, 0, 30, 0, 60000));
  printf("map(%d, 0, 30, 0, 888888) = %d\n", x,
         linlin_uint32_t(x, 0, 30, 0, 888888));
  return 0;
}
