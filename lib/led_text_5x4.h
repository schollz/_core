// adapted from https://github.com/tompreston/4x5-Font/tree/master
// MIT License
//
// Copyright (c) 2016 Thomas Preston <thomasmarkpreston@gmail.com>
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
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
#ifndef LIB_LED_TEXT_5X4_H
#define LIB_LED_TEXT_5X4_H 1
#include <stdbool.h>
#include <stdio.h>

// Define a structure to hold character and its corresponding 5x3 dot matrix
typedef struct {
  char character;
  unsigned char dots[5];
} CharMap;

// Define the character map
const CharMap led_text_5x4[] = {
    {'0', {0x6, 0x9, 0x9, 0x9, 0x6}}, {'1', {0x2, 0x6, 0x2, 0x2, 0x2}},
    {'2', {0xe, 0x1, 0x6, 0x8, 0xf}}, {'3', {0xe, 0x1, 0x6, 0x1, 0xe}},
    {'4', {0x2, 0x6, 0xa, 0xf, 0x2}}, {'5', {0xf, 0x8, 0xe, 0x1, 0xe}},
    {'6', {0x6, 0x8, 0xe, 0x9, 0x6}}, {'7', {0xf, 0x1, 0x2, 0x4, 0x8}},
    {'8', {0x6, 0x9, 0x6, 0x9, 0x6}}, {'9', {0x6, 0x9, 0xf, 0x1, 0x6}},
    {'v', {0x0, 0x9, 0x9, 0x6, 0x6}}, {'.', {0x0, 0x0, 0x0, 0x0, 0x4}},
    {' ', {0x0, 0x0, 0x0, 0x0, 0x0}}, {'z', {0x0, 0xf, 0x2, 0x4, 0xf}},
    {'A', {0x4, 0xa, 0xe, 0xa, 0xa}}, {'B', {0xe, 0x9, 0xe, 0x9, 0xe}},
    {'C', {0x6, 0x9, 0x8, 0x9, 0x6}}, {'D', {0xe, 0x9, 0x9, 0x9, 0xe}},
    {'E', {0xf, 0x8, 0xe, 0x8, 0xf}}, {'F', {0xf, 0x8, 0xe, 0x8, 0x8}},
    {'G', {0x6, 0x8, 0xb, 0x9, 0x6}}, {'H', {0x9, 0x9, 0xf, 0x9, 0x9}},
    {'I', {0xe, 0x4, 0x4, 0x4, 0xe}}, {'J', {0x1, 0x1, 0x1, 0x9, 0x6}},
    {'K', {0x9, 0xa, 0xc, 0xa, 0x9}}, {'L', {0x8, 0x8, 0x8, 0x8, 0xf}},
    {'M', {0x9, 0xf, 0xf, 0x9, 0x9}}, {'N', {0x9, 0xd, 0xf, 0xb, 0x9}},
    {'O', {0x6, 0x9, 0x9, 0x9, 0x6}}, {'P', {0xe, 0x9, 0xe, 0x8, 0x8}},
    {'Q', {0x6, 0x9, 0x9, 0xb, 0x7}}, {'R', {0xe, 0x9, 0xe, 0xa, 0x9}},
    {'S', {0x7, 0x8, 0x6, 0x1, 0xe}}, {'T', {0xe, 0x4, 0x4, 0x4, 0x4}},
    {'U', {0x9, 0x9, 0x9, 0x9, 0x6}}, {'V', {0x9, 0x9, 0x9, 0x6, 0x6}},
    {'W', {0x9, 0x9, 0xf, 0xf, 0x9}}, {'X', {0x9, 0x9, 0x6, 0x9, 0x9}},
    {'Y', {0x9, 0x5, 0x2, 0x2, 0x2}}, {'Z', {0xf, 0x2, 0x4, 0x8, 0xf}},
};

// Function to print the dot matrix of a character
void printCharDots(char c) {
  for (int i = 0; i < sizeof(led_text_5x4) / sizeof(led_text_5x4[0]); i++) {
    if (led_text_5x4[i].character == c) {
      for (int j = 0; j < 5; j++) {
        // print out 0 or 1
        for (int k = 0; k < 4; k++) {
          bool is_one = (led_text_5x4[i].dots[j] >> (3 - k)) & 1;
          if (is_one) {
            printf("█");
          } else {
            printf("0");
          }
        }
        printf("\n");
      }
      printf("\n");
      break;
    }
  }
}

void reverseArray(char *array, int length) {
  int start = 0;
  int end = length - 1;
  while (start < end) {
    char temp = array[start];
    array[start] = array[end];
    array[end] = temp;
    start++;
    end--;
  }
}

int numberToCharArray(int number, char *output) {
  int i = 0;
  if (number == 0) {
    output[i++] = '0';
  } else {
    while (number > 0) {
      output[i++] =
          (number % 10) + '0';  // Convert digit to char and store in array
      number /= 10;
    }
  }
  output[i] = '\0';         // Null-terminate the string
  reverseArray(output, i);  // Reverse the array to get correct order

  return i;  // Return the number of digits
}

#endif