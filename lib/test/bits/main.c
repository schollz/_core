// Copyright 2023-2025 Zack Scholl, GPLv3.0

#include <stdint.h>
#include <stdio.h>

void printBinaryRepresentation(unsigned int num) {
  // Iterate through each bit position (from 31 to 0)
  for (int i = 31; i >= 0; i--) {
    // Extract the i-th bit using bitwise operations
    uint8_t bit = (num >> i) & 1;
    printf("%u", bit);  // Print the bit
  }
  printf("\n");
}

void test2(uint8_t s) {
  uint8_t a[s];
  for (uint8_t i = 0; i < s; i++) {
    printf("%d %d\n", i, a[s]);
  }
}

// Round to nearest 5:
uint16_t round_uint16_to(uint16_t num, uint16_t multiple) {
  return (((2 * num) + multiple) / (2 * multiple)) * multiple;
}

int main() {
  unsigned int num1;
  unsigned int num2;

  num1 = 12;
  num2 = 8;

  printBinaryRepresentation(num1);
  printBinaryRepresentation(num2);
  // printf("%d\n", num << 8u);
  // printf("%d\n", (num << 8u) + ((num << 8u) >> 16u));

  test2(2);
  test2(5);

  printf("%d\n", round_uint16_to(35, 24));
  printf("%d\n", round_uint16_to(0, 1));
  printf("%d\n", round_uint16_to(1, 1));

  return 0;
}