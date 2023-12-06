#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "pico/stdlib.h"

#include "../fixedpoint.h"

int main() {
  stdio_init_all();

  sleep_ms(1000);

  printf("pico performance tests\n");

  uint32_t t0, t1;
  uint32_t n = 10000;

  t0 = time_us_32();
  float f = 0.0;
  for (int32_t i = 0; i < n; i++) {
    f = ((float)i) / 2.0;
  }
  t1 = time_us_32();
  printf("float it/s: %2.1f\n", (float)(t1 - t0) / (float)n * 1000000);

  t0 = time_us_32();
  float f = 0.0;
  int32_t cons = q16_16_float_to_fp(0.5);
  for (int32_t i = 0; i < n; i++) {
    f = q16_16_fp_to_float(q16_16_multiply(q16_16_int32_to_fp(i), cons));
  }
  t1 = time_us_32();
  printf("fp it/s: %2.1f\n", (float)(t1 - t0) / (float)n * 1000000);

  return 0;
}
