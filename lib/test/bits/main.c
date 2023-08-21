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

int main() {
  unsigned int num;

  printf("Enter a 32-bit unsigned integer: ");
  scanf("%u", &num);

  printf("Binary representation: ");
  printBinaryRepresentation(num);

  return 0;
}