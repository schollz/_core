#include <stdint.h>
extern struct zd_test_rosc { uint32_t randombit; } zd_test_rosc;
#define rosc_hw (&zd_test_rosc)
