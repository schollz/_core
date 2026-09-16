#ifndef SEEK_AUDIO_FIXTURE_H
#define SEEK_AUDIO_FIXTURE_H
#include "ff.h"
#include <stdint.h>
#if SEEK_TEST_AUDIO_FIXTURE
#include <stdatomic.h>
typedef struct {
    _Atomic uint32_t complete;
    uint32_t result, mode, operation, bytes, starting_cluster, elapsed_us, reserved;
} seek_audio_fixture_report;
extern seek_audio_fixture_report zeptocore_audio_fixture;
// Test boot only, with exclusive filesystem ownership. 1 adds/reuses a copy,
// 2 removes only a copy matching its saved CID, cluster, length and SHA-256.
// Modes 3/4 do the same for the companion stretched variation (.1 -> .3).
FRESULT seek_audio_fixture_run(unsigned mode,const uint8_t cid[16]);
#endif
#endif
