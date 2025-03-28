#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

// Function to store values
uint16_t storeValues(int value1, int value2) {
  // Ensure values fit in their respective bit sizes
  value1 = value1 & 0x1FF;  // 9 bits for value1
  value2 = value2 & 0x7F;   // 7 bits for value2

  // Store the values in the 16-bit integer
  return (value1 << 7) | value2;
}

// Function to retrieve the first value
int getValue1(uint16_t storedValue) {
  return (storedValue >> 7) & 0x1FF;  // Shift right 7 bits and mask with 9 bits
}

// Function to retrieve the second value
int getValue2(uint16_t storedValue) {
  return storedValue & 0x7F;  // Mask with 7 bits
}

// Function to set a boolean value
void setBool(uint8_t *value, int position, bool flag) {
  if (flag) {
    *value |= 1 << position;  // Set the bit at 'position' to 1
  } else {
    *value &= ~(1 << position);  // Set the bit at 'position' to 0
  }
}

// Function to get a boolean value
bool getBool(uint8_t value, int position) {
  return (value >> position) & 1;  // Retrieve the bit at 'position'
}
// Main function to test the above implementation
int main() {
  uint16_t stored = storeValues(
      123, 32);  // Use 65 for the second value, as 234 exceeds 7 bits

  printf("Stored Value: %u\n", stored);
  printf("Retrieved Value 1: %d\n", getValue1(stored));
  printf("Retrieved Value 2: %d\n", getValue2(stored));

  uint8_t myValue = 0;  // Initialize with all bits set to 0

  // Set some boolean values
  setBool(&myValue, 0, true);
  setBool(&myValue, 1, false);
  setBool(&myValue, 2, true);
  // ... set other bits as needed

  // Get and print some boolean values
  printf("Value at position 0: %s\n", getBool(myValue, 0) ? "true" : "false");
  printf("Value at position 1: %s\n", getBool(myValue, 1) ? "true" : "false");
  printf("Value at position 2: %s\n", getBool(myValue, 2) ? "true" : "false");
  // ... re
  return 0;
}
