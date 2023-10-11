// Copyright 2023 Zack Scholl.
//
// Author: Zack Scholl (zack.scholl@gmail.com)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// See http://creativecommons.org/licenses/MIT/ for more information.

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
  return 0;
}