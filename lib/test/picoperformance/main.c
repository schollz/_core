#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../../fixedpoint.h"
#include "pico/stdlib.h"

int main() {
  stdio_init_all();

  while (true) {
    sleep_ms(2000);
    printf("\n\npico performance tests\n");
    float f, g;
    uint32_t t0, t1;
    int32_t n = 22340;

    t0 = time_us_32();
    float const5 = 1.25;
    float const6 = 1.33;
    for (int32_t i = 0; i < n; i++) {
      float ff = ((float)i) * const5;
      float gg = ((float)i) * const6;
      ff += gg;
      f = ff;
      if (i == 1000) {
        printf("f=%2.1f\n", f);
      }
    }
    t1 = time_us_32();
    printf("fl it/ms: %10d\n",
           (uint32_t)round((float)n / (float)(t1 - t0) * 1000));

    t0 = time_us_32();
    int32_t cons = q16_16_float_to_fp(1.25);
    int32_t cons2 = q16_16_float_to_fp(1.33);
    for (int32_t i = 0; i < n; i++) {
      int32_t ff = q16_16_multiply(q16_16_int32_to_fp(i), cons);
      int32_t gg = q16_16_multiply(q16_16_int32_to_fp(i), cons2);
      ff += gg;
      f = q16_16_fp_to_float(ff);
      if (i == 1000) {
        printf("f=%2.1f\n", f);
      }
    }
    t1 = time_us_32();
    printf("fp it/ms: %10d\n",
           (uint32_t)round((float)n / (float)(t1 - t0) * 1000));
    printf("\n");
    sleep_ms(8000);
  }
  return 0;
}
