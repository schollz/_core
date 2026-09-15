// Copyright 2026 Zack Scholl, GPLv3.0
#ifndef AUDIO_SEEK_MAP_H
#define AUDIO_SEEK_MAP_H
#include "ff.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>
#define SEEK_MAP_WORDS 64u
#define SEEK_MAP_CACHE_ENTRIES 2u
#define SEEK_MAP_FILES 256u
#define SEEK_MAP_RECORDS 512u
#ifndef SEEK_MAP_DIRECTORY
#define SEEK_MAP_DIRECTORY ".core_seek"
#endif
typedef enum { SEEK_MAP_IDLE, SEEK_MAP_LOAD, SEEK_MAP_PREPARE } seek_map_job;
typedef struct {
    _Atomic uint32_t mount, files, builds, reused, validated, invalid, oversized;
    _Atomic uint32_t loads, hits, misses, evictions, writes, commits, failures, capacity;
    _Atomic uint32_t validation_us, build_us, load_us, commit_us, prepare_us;
    _Atomic uint32_t retained_bytes, reserved_bytes, generation, pending, last_error;
} seek_map_stats;
extern seek_map_stats seek_maps_stats;
// All functions require exclusive filesystem ownership. Attach/detach/request
// are bounded RAM-only operations, also allowed to the playback filesystem owner.
// prepare/unmount require every attached handle to have been detached first.
// FR_DISK_ERR from prepare requires unmount/remount before ordinary playback;
// a failed index write may leave FatFs's shared metadata window dirty.
FRESULT seek_maps_prepare(FATFS *fs, const uint8_t cid[16], uint64_t sectors,
                         const char *initial_file);
bool seek_maps_unmount(void);
bool seek_maps_media_ready(void);
bool seek_maps_attach(FIL *file, const char *path);
void seek_maps_detach(FIL *file);
seek_map_job seek_maps_request(const char *path);
// Caller must already have stopped playback AND acquired its acknowledgement.
void seek_maps_service(void);
// Call before a firmware operation can change this audio file's allocations.
// The caller must detach/close affected handles before that operation.
bool seek_maps_invalidate(const char *path);
bool seek_maps_file_id(const char *path, uint16_t *id);
void seek_maps_path(uint16_t id, char path[32]);
// Exception to the ownership rule: call only from core 0's foreground
// diagnostic publisher, which cannot overlap its own cache writer. No I/O,
// allocation, ownership request or cache load. Writes 76 snapshot words.
void seek_maps_layout(uint16_t id, volatile uint32_t out[76]);
#endif
