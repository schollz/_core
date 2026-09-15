// Test firmware only: automatic boot work, never initiated by diagnostics.
#ifndef SEEK_FRAGMENT_BENCHMARK_H
#define SEEK_FRAGMENT_BENCHMARK_H
#include "ff.h"
#if SEEK_TEST_FRAGMENT_BENCH
#include "seek_diagnostics.h"
#include <stdatomic.h>
typedef struct {
    _Atomic uint32_t complete;
    // schema, result, cluster sectors, size, fragments, table words, pairs,
    // created, build us, setup us, ordinary/mapped FAT visits, verified bytes,
    // saturation, start cluster, then reserved.
    uint32_t context[31];
    zd_metric_t ordinary_seek, ordinary_read, mapped_seek, mapped_read;
} seek_benchmark_report;
_Static_assert(sizeof(seek_benchmark_report)==512,"benchmark report ABI");
extern seek_benchmark_report zeptocore_seek_benchmark;
extern DWORD seek_benchmark_count_fat, seek_benchmark_fat_visits;
// Reuse the preparation arena before its index is opened. All handles closed
// on return. Caller has the existing exclusive boot media ownership.
FRESULT seek_fragment_benchmark(FATFS *fs,FIL *file,FIL *spacing,
                                DWORD table[64],BYTE data[512]);
#endif
#endif
