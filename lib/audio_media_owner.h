// Copyright 2026 Zack Scholl, GPLv3.0
#ifndef AUDIO_MEDIA_OWNER_H
#define AUDIO_MEDIA_OWNER_H
#include <stdbool.h>
#include <stdint.h>

// Core 0 starts with a nested ownership depth of one. Release this boot guard
// only after media, DSP and control initialization have all completed.
void audio_media_boot_complete(void);
bool audio_media_acquire(void);
bool audio_media_try_quiet(void);
void audio_media_release(void);
bool audio_media_control_owned(void);
bool audio_media_timer_allowed(void);

// Core 1: begin false means service output without touching playback state.
// Acknowledge at end, AFTER every file access and diagnostic metadata read.
bool audio_media_begin(void);
void audio_media_end(bool quiescent);

typedef struct {
    uint32_t acquisitions, timeouts, quiet_rejected, wait_max_us;
    uint32_t withheld_callbacks;
} audio_media_owner_stats;
extern audio_media_owner_stats audio_media_stats;
#endif
