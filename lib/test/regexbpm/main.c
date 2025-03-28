// Copyright 2023-2025 Zack Scholl, GPLv3.0

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
