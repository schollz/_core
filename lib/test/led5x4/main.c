// Copyright 2023-2025 Zack Scholl, GPLv3.0
#define IS_DESKTOP 1

#include "../../led_text_5x4.h"

int main() {
  int number = 103;
  char output[3];  // Assuming the number won't have more than 3 digits

  int numDigits = numberToCharArray(number, output);
  for (int i = 0; i < numDigits; i++) {
    printCharDots(output[i]);
  }

  return 0;
}
