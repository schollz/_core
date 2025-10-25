// Copyright 2023-2025 Zack Scholl, GPLv3.0

#ifndef PERSISTENT_STATE_LIB
#define PERSISTENT_STATE_LIB 1

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// Magic number to validate flash data integrity (ASCII for "CORE")
#define PERSISTENT_STATE_MAGIC 0x434F5245

// Structure to persist bank and sample selection across restarts
typedef struct {
  uint32_t magic;           // Magic number for validation
  uint8_t bank;             // Current bank (0-15)
  uint8_t sample;           // Current sample (0-15)
  uint16_t checksum;        // Simple checksum for additional validation
  uint32_t padding;         // Padding for alignment
} PersistentState;

// Calculate a simple checksum for the state
uint16_t __not_in_flash_func(PersistentState_calculate_checksum)(PersistentState *state) {
  uint16_t sum = 0;
  sum += state->magic & 0xFFFF;
  sum += (state->magic >> 16) & 0xFFFF;
  sum += state->bank;
  sum += state->sample;
  return sum;
}

// Save the current bank and sample to flash
void __not_in_flash_func(PersistentState_save)(uint8_t bank, uint8_t sample) {
  PersistentState state;
  state.magic = PERSISTENT_STATE_MAGIC;
  state.bank = bank;
  state.sample = sample;
  state.checksum = PersistentState_calculate_checksum(&state);
  state.padding = 0;
  
  printf("[PersistentState] Saving bank=%d sample=%d to flash\n", bank, sample);
  write_struct_to_flash(&state, sizeof(PersistentState));
}

// Load and validate the bank and sample from flash
// Returns true if valid data was loaded, false otherwise
bool PersistentState_load(uint8_t *bank, uint8_t *sample, uint8_t max_banks, uint8_t *banks_with_samples, uint8_t banks_with_samples_num, SampleList **banks_list) {
  PersistentState state;
  read_struct_from_flash(&state, sizeof(PersistentState));
  
  // Validate magic number
  if (state.magic != PERSISTENT_STATE_MAGIC) {
    printf("[PersistentState] Invalid magic number: 0x%08X (expected 0x%08X)\n", 
           state.magic, PERSISTENT_STATE_MAGIC);
    return false;
  }
  
  // Validate checksum
  uint16_t expected_checksum = PersistentState_calculate_checksum(&state);
  if (state.checksum != expected_checksum) {
    printf("[PersistentState] Invalid checksum: 0x%04X (expected 0x%04X)\n", 
           state.checksum, expected_checksum);
    return false;
  }
  
  // Validate bank is within bounds
  if (state.bank >= max_banks) {
    printf("[PersistentState] Bank %d out of bounds (max %d)\n", 
           state.bank, max_banks - 1);
    return false;
  }
  
  // Check if the bank exists (has samples)
  bool bank_exists = false;
  for (uint8_t i = 0; i < banks_with_samples_num; i++) {
    if (banks_with_samples[i] == state.bank) {
      bank_exists = true;
      break;
    }
  }
  
  if (!bank_exists) {
    printf("[PersistentState] Bank %d has no samples\n", state.bank);
    return false;
  }
  
  // Validate sample exists in the bank
  if (state.sample >= banks_list[state.bank]->num_samples) {
    printf("[PersistentState] Sample %d out of bounds for bank %d (max %d)\n", 
           state.sample, state.bank, banks_list[state.bank]->num_samples - 1);
    return false;
  }
  
  *bank = state.bank;
  *sample = state.sample;
  
  printf("[PersistentState] Successfully loaded bank=%d sample=%d from flash\n", 
         *bank, *sample);
  return true;
}

#endif
