// Copyright 2026 Zack Scholl. SPDX-License-Identifier: GPL-3.0-only
#ifndef CORE_ENGINE_H
#define CORE_ENGINE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
enum { CORE_RATE = 44100, CORE_BLOCK = 441, CORE_KNOBS = 5, CORE_BUTTONS = 4 };
typedef struct CoreEngine CoreEngine;
// Knobs: Break, Grimoire, Amen, Amen random, Sample. Buttons: Mode, Mult, Bank, Tap.
typedef struct {
    float knobs[5], cv[3]; // normalized knobs; CV in volts, Amen/Break/Sample
    bool buttons[4], connected[3];
} CoreControls;
typedef struct {
    uint32_t size;
    uint16_t bpm, splice_ticks;
    uint8_t slot, channels, rate_multiple, mode, slice_count;
    bool one_shot, tempo_match, variable;
    const int32_t *starts, *stops;
    const int8_t *types;
    uint8_t transient_count[3];
    const uint16_t *transients[3];
} CoreSample;
typedef struct { uint8_t count; CoreSample samples[16]; } CoreBank;
// Called synchronously by the engine. Must only copy resident PCM/cache pages;
// a cache miss returns false and queues IO in the owner's non-realtime worker.
typedef bool (*CoreRead)(void *, unsigned bank, unsigned slot, unsigned variant,
                         uint64_t byte_offset, void *dst, size_t bytes);
typedef struct {
    uint32_t version;
    uint16_t tempo, volume;
    uint8_t bank, slot, rune, division, trigger_mode, pitch;
    bool stopped, muted, stay_in_sync;
    bool effects[16];
    uint8_t effect_params[16][3];
    uint8_t brightness, amen_behavior, sample_mapping;
    bool bipolar[3], clock_stop, clock_trigger, clock_slice;
    int8_t reset_input;
    bool runes[7][16];
    uint8_t amiga, saturation, smear, jitter, chaos, jump_probability;
    uint8_t retrigger_probability;
    uint16_t filter_index;
    uint8_t filter_type;
    uint64_t random_state, random_increment;
    uint8_t sequence[64], sequence_length;
} CoreState;
typedef struct {
    uint8_t rgb[18][3];
    bool mode[4], tap, clock, trigger, stopped, muted;
    uint8_t bank, slot, slice, rune;
    uint16_t tempo, effects;
    unsigned requested_bank;
    bool reboot_requested;
    uint64_t underruns;
} CoreDisplay;
CoreEngine *core_engine_create(uint64_t seed);
void core_engine_destroy(CoreEngine *);
void core_engine_set_catalogue(CoreEngine *, const CoreBank banks[16], CoreRead, void *);
void core_engine_set_resident_bank(CoreEngine *, unsigned bank);
void core_engine_controls(CoreEngine *, const CoreControls *);
void core_engine_clock(CoreEngine *, bool high);
// Advance one 44.1 kHz frame; returns original 16-bit stereo PCM.
void core_engine_process(CoreEngine *, int16_t out[2]);
void core_engine_display(CoreEngine *, CoreDisplay *);
void core_engine_get_state(CoreEngine *, CoreState *);
bool core_engine_set_state(CoreEngine *, const CoreState *);
bool core_engine_update_settings(CoreEngine *, const CoreState *);
void core_engine_clear_holds(CoreEngine *);
#ifdef __cplusplus
}
#endif
#endif
