
// Define the flash storage offset (make sure this does not overlap with your
// program's code or other data)
#define FLASH_TARGET_OFFSET 1572864
// Function to write arbitrary C struct to flash memory
void write_struct_to_flash(const void* data, size_t size) {
  // Ensure the data size is not larger than a sector (usually 4096 bytes)
  if (size > FLASH_SECTOR_SIZE) {
    // Handle error: data size is too large to write in one sector
    return;
  }

  // Compute the flash address
  uint32_t flash_address = XIP_BASE + FLASH_TARGET_OFFSET;

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

// Example struct to demonstrate usage
typedef struct {
  int id;
  float value;
  char name[32];
} MyStruct;

MyStruct test_flash() {
  // Create an example struct to write to flash
  MyStruct write_data = {123, 456.78f, "Hello Flash"};

  // Write the struct to flash
  write_struct_to_flash(&write_data, sizeof(write_data));

  // Create a struct to read the data back into
  MyStruct read_data;

  // Read the struct from flash
  read_struct_from_flash(&read_data, sizeof(read_data));

  return read_data;
}