// Optional, bounded callback attribution. Times are inclusive where noted.
#ifndef AUDIO_PROFILE_H
#define AUDIO_PROFILE_H
#include <stdint.h>
#include <stdbool.h>
enum { AP_OPEN, AP_CLOSE, AP_SEEK, AP_READ, AP_STRETCH_SEEK, AP_STRETCH_READ,
       AP_BEAT, AP_SATURATE, AP_SHAPER, AP_FUZZ, AP_BITCRUSH, AP_RESAMPLE,
       AP_FILTER, AP_PAN, AP_REVERB, AP_DELAY_SETUP, AP_DELAY, AP_COMB,
       AP_DIGITAL, AP_CONVERT, AP_STRETCH, AP_COUNT };
typedef struct {
    uint32_t sequence, callback, start_us, total_us, source_before, source_after;
    uint32_t effects_before, effects_after, pitch, stretch, event, event_us, relation;
    uint32_t us[AP_COUNT];
} audio_profile_record;
#if AUDIO_DETAILED_TIMING
#include "pico/time.h"
extern volatile audio_profile_record audio_profile_records[4];
void audio_profile_begin(bool active,uint32_t source,uint32_t effects,
                         uint32_t pitch,uint32_t stretch);
void audio_profile_end(uint32_t source,uint32_t effects);
void audio_profile_add(unsigned stage,uint32_t started);
void audio_profile_starved(uint32_t now);
#define AP_START(name) uint32_t name=time_us_32()
#define AP_END(stage,name) audio_profile_add(stage,name)
#define AP_RESET(name) name=time_us_32()
#define AP_MEASURE(stage,...) do { uint32_t ap_clock=time_us_32(); __VA_ARGS__; audio_profile_add(stage,ap_clock); } while(0)
#define AP_CALL(...) __VA_ARGS__
#else
#define AP_START(name) ((void)0)
#define AP_END(stage,name) ((void)0)
#define AP_RESET(name) ((void)0)
#define AP_MEASURE(stage,...) do { __VA_ARGS__; } while(0)
#define AP_CALL(...) ((void)0)
#endif
#endif
