// Copyright 2026 Zack Scholl, GPLv3.0
#ifndef BANK_METADATA_H
#define BANK_METADATA_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "sampleinfo.h"
#ifndef FILE_VARIATIONS
#define FILE_VARIATIONS 2
#endif
typedef struct Sample { SampleInfo *snd[FILE_VARIATIONS]; } Sample;
typedef struct SampleList {
    uint16_t num_samples, valid_slots;
    Sample *sample; // knob ordinals, in ascending physical filename order
    size_t detail_bytes;
} SampleList;
extern SampleList *banks[16];

enum MetadataError {
    META_OK, META_IO, META_TRUNCATED, META_FORMAT, META_RANGE,
    META_MEMORY, META_CHANGED, META_TIMEOUT, META_CANCELLED, META_AUDIO
};
// Independent SWD symbol. The existing diagnostic mailbox layout is unchanged.
typedef struct {
    uint32_t resident_bank, loading_bank, generation, arena_capacity, arena_used;
    uint32_t minimum_free_heap, transition_us, maximum_transition_us;
    uint32_t rollbacks, last_error, state, rejected_files;
} MetadataStatus;
extern volatile MetadataStatus metadata_status;

bool metadata_catalogue_scan(void);
void metadata_catalogue_destroy(void);
bool metadata_reserve(void);
bool metadata_ready(unsigned bank);
unsigned metadata_generation(void);
uint8_t metadata_filename_index(unsigned bank, unsigned ordinal);
int metadata_ordinal(unsigned bank, unsigned filename_index);
bool metadata_selection_valid(unsigned bank, unsigned sample);
// Called only with exclusive filesystem/audio ownership. No allocation in load.
bool metadata_load_begin(unsigned bank, uint32_t now);
// Exactly zero/one file operation, with reads bounded to 512 bytes.
// Returns 0 while pending, 1 complete (unpublished), -1 on error.
int metadata_load_step(uint32_t now);
void metadata_load_cancel(void); // one close at most; discards staged views
void metadata_publish(void);     // no IO; publish only a complete bank
#endif
