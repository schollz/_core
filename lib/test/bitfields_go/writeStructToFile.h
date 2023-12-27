#ifndef WRITESTRUCTTOFILE_H
#define WRITESTRUCTTOFILE_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// Example struct
typedef struct SomeStruct {
  uint8_t a : 4;
  uint8_t b : 4;
  uint8_t num;
  uint8_t *arr;
} SomeStruct;

int writeStructToFile(uint8_t a, uint8_t b, uint8_t num, uint8_t *arr) {
  SomeStruct *data = malloc(sizeof(SomeStruct));
  if (data == NULL) {
    perror("Error allocating memory");
    return -1;
  }

  data->a = a;
  data->b = b;
  data->num = num;
  data->arr = (uint8_t *)malloc(sizeof(uint8_t) * num);
  for (uint8_t i = 0; i < num; i++) {
    data->arr[i] = arr[i];
  }
  FILE *file = fopen("test.bin", "wb");
  if (file == NULL) {
    perror("Error opening file");
    free(data);
    return -1;
  }

  // Write the struct (excluding the array)
  if (fwrite(data, sizeof(SomeStruct) - sizeof(uint8_t *), 1, file) != 1) {
    perror("Error writing struct to file");
    fclose(file);
    free(data->arr);
    free(data);
    return -1;
  }

  // Write the array content
  if (fwrite(data->arr, sizeof(uint8_t), num, file) != num) {
    perror("Error writing array to file");
    fclose(file);
    free(data->arr);
    free(data);
    return -1;
  }

  fclose(file);
  free(data->arr);
  free(data);
  return 0;
}

SomeStruct *readStructFromFile() {
  FILE *file = fopen("test.bin", "rb");
  if (file == NULL) {
    perror("Error opening file");
    return NULL;
  }

  SomeStruct *data = malloc(sizeof(SomeStruct));
  if (data == NULL) {
    perror("Error allocating memory");
    fclose(file);
    return NULL;
  }

  // Read the struct (excluding the array pointer)
  if (fread(data, sizeof(SomeStruct) - sizeof(uint8_t *), 1, file) != 1) {
    perror("Error reading struct from file");
    fclose(file);
    free(data);
    return NULL;
  }

  // Allocate memory for the array
  data->arr = (uint8_t *)malloc(sizeof(uint8_t) * data->num);
  if (data->arr == NULL) {
    perror("Error allocating memory for array");
    fclose(file);
    free(data);
    return NULL;
  }

  // Read the array content
  if (fread(data->arr, sizeof(uint8_t), data->num, file) != data->num) {
    perror("Error reading array from file");
    fclose(file);
    free(data->arr);
    free(data);
    return NULL;
  }

  fclose(file);
  return data;
}

uint8_t getA(SomeStruct *s) { return s->a; }

uint8_t getB(SomeStruct *s) { return s->b; }

uint8_t getArr(SomeStruct *s, uint8_t i) { return s->arr[i]; }
#endif  // WRITESTRUCTTOFILE_H
