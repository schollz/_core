// Copyright 2023-2025 Zack Scholl, GPLv3.0

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