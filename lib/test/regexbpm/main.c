// Copyright 2023-2024 Zack Scholl.
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

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Function to extract the number following "bpm" in the input string
unsigned int extractBPM(const char *input) {
  int len = strlen(input);
  int i = 0;
  bool foundBPM = false;

  while (i < len) {
    // Check for the pattern "bpm"
    if (input[i] == 'b' && input[i + 1] == 'p' && input[i + 2] == 'm') {
      foundBPM = true;
      i += 3;  // Skip "bpm"
      break;
    }
    i++;
  }

  // If "bpm" is found, extract the number
  if (foundBPM) {
    char numberBuffer[10];
    int j = 0;

    while (i < len && isdigit(input[i])) {
      numberBuffer[j] = input[i];
      i++;
      j++;
    }

    numberBuffer[j] = '\0';
    int bpmValue = atoi(numberBuffer);

    return bpmValue;
  } else {
    return -1;  // If "bpm" is not found, return -1 to indicate failure
  }
}

int main() {
  char inputString[] = "thebreakbeat_bpm120_something.wav";
  unsigned int bpmValue = extractBPM(inputString);

  if (bpmValue != -1) {
    printf("Extracted BPM value: %d\n", bpmValue);
  } else {
    printf("BPM not found in the input string.\n");
  }

  return 0;
}
