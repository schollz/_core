
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

/// Convert seconds to milliseconds
#define SEC_TO_MS(sec) ((sec)*1000)
/// Convert seconds to microseconds
#define SEC_TO_US(sec) ((sec)*1000000)
/// Convert seconds to nanoseconds
#define SEC_TO_NS(sec) ((sec)*1000000000)

/// Convert nanoseconds to seconds
#define NS_TO_SEC(ns) ((ns) / 1000000000)
/// Convert nanoseconds to milliseconds
#define NS_TO_MS(ns) ((ns) / 1000000)
/// Convert nanoseconds to microseconds
#define NS_TO_US(ns) ((ns) / 1000)
// https://stackoverflow.com/questions/5833094/get-a-timestamp-in-c-in-microseconds
uint64_t micros() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
  uint64_t us = SEC_TO_US((uint64_t)ts.tv_sec) + NS_TO_US((uint64_t)ts.tv_nsec);
  return us;
}

uint32_t time_us_32() { return (micros() / 1000) % 10000000; }

uint64_t time_us_64() { return time_us_32(); }

#include "../../pcg_basic.h"
#include "../../random.h"
#include "../../random_seeded.h"

int main() {
  printf("Hello, World!\n");
  random_initialize();
  uint32_t seed = random_integer_in_range(0, 4294967294);
  printf("seed: %d\n", seed);
  random_seeded_initialize(seed);
  uint8_t num_numbers = random_seeded_integer_in_range(1, 8) * 4;
  uint8_t random_sequence[32];
  for (uint8_t i = 0; i < num_numbers; i++) {
    random_sequence[i] = random_seeded_integer_in_range(0, 100);
    printf("%d ", random_sequence[i]);
  }
  printf("\n");

  for (uint8_t i = 0; i < num_numbers; i++) {
    printf("%d ", random_integer_in_range(0, 1));
  }
  return 0;
}
