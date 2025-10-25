// Copyright 2023-2025 Zack Scholl, GPLv3.0

#ifndef PERSISTENT_STATE_LIB
#define PERSISTENT_STATE_LIB 1

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// Magic number to validate flash data integrity (ASCII for "CORE")
#define PERSISTENT_STATE_MAGIC 0x434F5245

// Combined structure to persist both calibration data and bank/sample selection
// This structure is shared between ectocore calibration and persistent state
typedef struct {
  uint32_t magic;                // Magic number for validation (0x434F5245)
  uint16_t center_calibration[8]; // Calibration data for ectocore (16 bytes)
  uint8_t bank;                  // Current bank (0-15)
  uint8_t sample;                // Current sample (0-15)
  uint16_t checksum;             // Simple checksum for additional validation
  uint32_t padding;              // Padding for alignment
} PersistentState;

// Calculate a simple checksum for the state
uint16_t __not_in_flash_func(PersistentState_calculate_checksum)(PersistentState *state) {
  uint16_t sum = 0;
  sum += state->magic & 0xFFFF;
  sum += (state->magic >> 16) & 0xFFFF;
  // Include calibration data in checksum
  for (uint8_t i = 0; i < 8; i++) {
    sum += state->center_calibration[i];
  }
  sum += state->bank;
  sum += state->sample;
  return sum;
}

// Save the current bank and sample to flash (preserves calibration data)
void __not_in_flash_func(PersistentState_save)(uint8_t bank, uint8_t sample) {
  PersistentState state;
  
  // First, read existing data to preserve calibration
  read_struct_from_flash(&state, sizeof(PersistentState));
  
  // Check if we have valid existing data with calibration
  bool has_valid_calibration = false;
  if (state.magic == PERSISTENT_STATE_MAGIC) {
    // Validate calibration data is within reasonable bounds
    has_valid_calibration = true;
    for (uint8_t i = 0; i < 8; i++) {
      if (state.center_calibration[i] > 1024) {
        has_valid_calibration = false;
        break;
      }
    }
  }
  
  // If no valid calibration data exists, initialize with defaults
  if (!has_valid_calibration) {
    for (uint8_t i = 0; i < 8; i++) {
      state.center_calibration[i] = 1024 / 2;
    }
  }
  
  // Update bank and sample
  state.magic = PERSISTENT_STATE_MAGIC;
  state.bank = bank;
  state.sample = sample;
  state.checksum = PersistentState_calculate_checksum(&state);
  state.padding = 0;
  
  printf("[PersistentState] Saving bank=%d sample=%d to flash (preserving calibration)\n", bank, sample);
  write_struct_to_flash(&state, sizeof(PersistentState));
}

// Load and validate the bank and sample from flash
// Returns true if valid data was loaded, false otherwise
bool __not_in_flash_func(PersistentState_load)(uint8_t *bank, uint8_t *sample, uint8_t max_banks, uint8_t *banks_with_samples, uint8_t banks_with_samples_num, SampleList **banks_list) {
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

// Save calibration data to flash (preserves bank/sample data)
void __not_in_flash_func(PersistentState_save_calibration)(uint16_t calibration[8]) {
  PersistentState state;
  
  // First, read existing data to preserve bank/sample
  read_struct_from_flash(&state, sizeof(PersistentState));
  
  // Check if we have valid existing bank/sample data
  bool has_valid_bank_sample = false;
  if (state.magic == PERSISTENT_STATE_MAGIC) {
    uint16_t expected_checksum = PersistentState_calculate_checksum(&state);
    if (state.checksum == expected_checksum && state.bank < 16 && state.sample < 16) {
      has_valid_bank_sample = true;
    }
  }
  
  // If no valid bank/sample data exists, initialize with defaults
  if (!has_valid_bank_sample) {
    state.bank = 0;
    state.sample = 0;
  }
  
  // Update calibration data
  state.magic = PERSISTENT_STATE_MAGIC;
  for (uint8_t i = 0; i < 8; i++) {
    state.center_calibration[i] = calibration[i];
  }
  state.checksum = PersistentState_calculate_checksum(&state);
  state.padding = 0;
  
  printf("[PersistentState] Saving calibration to flash (preserving bank/sample)\n");
  write_struct_to_flash(&state, sizeof(PersistentState));
}

// Load calibration data from flash
// Returns true if valid calibration data was loaded
bool __not_in_flash_func(PersistentState_load_calibration)(uint16_t calibration[8]) {
  PersistentState state;
  read_struct_from_flash(&state, sizeof(PersistentState));
  
  // Validate magic number
  if (state.magic != PERSISTENT_STATE_MAGIC) {
    printf("[PersistentState] Invalid magic number for calibration: 0x%08X\n", state.magic);
    return false;
  }
  
  // Validate checksum
  uint16_t expected_checksum = PersistentState_calculate_checksum(&state);
  if (state.checksum != expected_checksum) {
    printf("[PersistentState] Invalid checksum for calibration: 0x%04X (expected 0x%04X)\n", 
           state.checksum, expected_checksum);
    return false;
  }
  
  // Validate calibration data is within reasonable bounds
  for (uint8_t i = 0; i < 8; i++) {
    if (state.center_calibration[i] > 1024) {
      printf("[PersistentState] Calibration data out of bounds: %d\n", 
             state.center_calibration[i]);
      return false;
    }
  }
  
  // Copy calibration data
  for (uint8_t i = 0; i < 8; i++) {
    calibration[i] = state.center_calibration[i];
  }
  
  printf("[PersistentState] Successfully loaded calibration from flash\n");
  return true;
}

#endif
