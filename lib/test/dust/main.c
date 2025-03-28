
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
#include "../../utils.h"
//
#include "../../dust.h"

#define RANDOMPROCESS_NUM 8
Dust randomprocess[RANDOMPROCESS_NUM] = {
    {0, 0, 0, 0, NULL}, {0, 0, 0, 0, NULL}, {0, 0, 0, 0, NULL},
    {0, 0, 0, 0, NULL}, {0, 0, 0, 0, NULL}, {0, 0, 0, 0, NULL},
    {0, 0, 0, 0, NULL}, {0, 0, 0, 0, NULL},
};

void callback0() { printf("callback0\n"); }
void callback1() { printf("callback1\n"); }
int main() {
  printf("Hello, World!\n");
  printf("time_us_32(): %d\n", time_us_32());
  random_initialize();
  printf("random integer: %d\n", random_integer_in_range(1, 100));

  Dust_setFrequency(&randomprocess[0], 500);
  printf("randomprocess[0].duration: %d\n", randomprocess[0].duration);
  Dust_setFrequency(&randomprocess[1], 300);
  Dust_setCallback(&randomprocess[0], &callback0);
  Dust_setCallback(&randomprocess[1], &callback1);

  while (1) {
    // sleep(0.1);
    // emulate the pico
    sleep((float)random_integer_in_range(1, 10) / 1000.0);
    for (uint8_t i = 0; i < RANDOMPROCESS_NUM; i++) {
      Dust_update(&randomprocess[i]);
    }
    // printf("current: %ld, next firing: %d\n", time_us_32(),
    //        randomprocess[0].next_firing);

    // emulate the pico
    // sleep((float)random_integer_in_range(1,10)/1000.0);
    // printf("random integer: %d\n", random_integer_in_range(1, 100));
  }
  return 0;
}
