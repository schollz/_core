#ifndef FLASHMEM_LIB
#define FLASHMEM_LIB 1

#define PICO_FLASH_SECTOR_SIZE 4096
#define PICO_FLASH_ORIGIN 0x10000000
#define PICO_FLASH_LENGTH (2 * 1024 * 1024)
#define PICO_FLASH_LAST_SECTOR \
  (PICO_FLASH_ORIGIN + PICO_FLASH_LENGTH - PICO_FLASH_SECTOR_SIZE)
#define FLASH_TARGET_OFFSET (PICO_FLASH_LAST_SECTOR)

// Function to write arbitrary C struct to flash memory
void write_struct_to_flash(const void* data, size_t size) {
  // Ensure the data size is not larger than a sector (usually 4096 bytes)
  if (size > FLASH_SECTOR_SIZE) {
    // Handle error: data size is too large to write in one sector
    return;
  }

  // // Compute the flash address
  // uint32_t flash_address = XIP_BASE + FLASH_TARGET_OFFSET;

  // Disable interrupts to prevent flash access conflicts
  uint32_t interrupt_status = save_and_disable_interrupts();

  // Erase the flash sector before writing (sector size is 4096 bytes)
  flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);

  // Write the data to flash
  flash_range_program(FLASH_TARGET_OFFSET, (const uint8_t*)data, size);

  // Restore interrupts
  restore_interrupts(interrupt_status);
}

// Function to read arbitrary C struct from flash memory
void read_struct_from_flash(void* data, size_t size) {
  // Compute the flash address
  const uint8_t* flash_address =
      (const uint8_t*)(XIP_BASE + FLASH_TARGET_OFFSET);

  // Read the data from flash
  memcpy(data, flash_address, size);
}

#endif