#ifndef SCREEN_TEST_STDLIB_H
#define SCREEN_TEST_STDLIB_H
#include <stdint.h>
extern uint32_t screen_test_now;
static inline uint32_t time_us_32(void) { return screen_test_now; }
#endif
