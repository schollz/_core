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

#include <stdio.h>
#include <stdlib.h>

// Structure to store the character array and its size
#define char_array_max 255
typedef struct char_array {
  char **arr;
  int num;
} char_array;

// Function to get the character array
char_array *get_char_array() {
  char_array *array = malloc(sizeof(char_array));
  array->arr = malloc(sizeof(char *) * char_array_max);
  array->arr[0] = "a string";
  array->num = 1;

  // Add a new string to the array
  array->arr[array->num] = "another string!";
  array->num++;

  return array;
}

// Driver code
int main() {
  // Get the character array
  char_array *array = get_char_array();

  printf("%ld\n", sizeof(char *) * char_array_max);
  // Print the array
  for (int i = 0; i < array->num; i++) {
    printf("%s\n", array->arr[i]);
  }

  // Free the memory
  free(array->arr);
  free(array);

  return 0;
}