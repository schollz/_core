// Copyright 2026 Zack Scholl, GPLv3.0
#ifndef ZEPTOCORE_SCREEN_H
#define ZEPTOCORE_SCREEN_H
#include <stdint.h>
#include <stdbool.h>
#ifndef ZEPTOCORE_SCREEN
#define ZEPTOCORE_SCREEN 0
#endif
#if ZEPTOCORE_SCREEN
#define ZS_CALL(...) do { __VA_ARGS__; } while (0)
enum { ZS_VALID = 1, ZS_FORWARD = 2, ZS_STOPPED = 4, ZS_MUTED = 8 };
typedef struct {
  uint32_t generation, bank, slot, phase, size, bpm, slice, flags;
} screen_snapshot;
typedef struct {
  uint32_t frames, transfers_failed, cache_bank, cache_valid, loaded, missing,
           invalid, io_errors, loading_us, snapshot_misses, cache_no_memory;
} screen_status_t;
extern volatile screen_status_t screen_status;
void screen_init(void);
void screen_boot_startup(unsigned bank, unsigned selected_slot);
void screen_service(uint32_t now);
void screen_publish(const screen_snapshot *s); // audio core, bounded RAM only
void screen_waveforms_begin(unsigned bank, unsigned selected_slot, uint32_t now);
// Exclusive filesystem ownership; zero/one file operation per call.
// Completion includes the optional deadline; pending work can resume stopped.
bool screen_waveforms_step(uint32_t now, uint32_t deadline);
bool screen_waveforms_pending(void);
void screen_waveforms_cancel(void);
#else
#define ZS_CALL(...) ((void)0)
#endif
#endif
