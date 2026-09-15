#include <stdint.h>
extern uint32_t zd_test_time;
static inline uint32_t time_us_32(void) { return zd_test_time++; }
