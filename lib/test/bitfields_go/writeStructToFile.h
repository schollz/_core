#ifndef WRITESTRUCTTOFILE_H
#define WRITESTRUCTTOFILE_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// Example struct
typedef struct SomeStruct {
  uint8_t a : 4;
  uint8_t b : 4;
} SomeStruct;

int writeStructToFile(uint8_t a, uint8_t b) {
  SomeStruct *data = malloc(sizeof(SomeStruct));
  if (data == NULL) {
    perror("Error allocating memory");
    return -1;
  }

  data->a = a;
  data->b = b;

  FILE *file = fopen("test.bin", "wb");
  if (file == NULL) {
    perror("Error opening file");
    free(data);
    return -1;
  }

  if (fwrite(data, sizeof(SomeStruct), 1, file) != 1) {
    perror("Error writing struct to file");
    fclose(file);
    free(data);
    return -1;
  }

  fclose(file);
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

  if (fread(data, sizeof(SomeStruct), 1, file) != 1) {
    perror("Error reading struct from file");
    fclose(file);
    free(data);
    return NULL;
  }

  fclose(file);
  return data;
}

uint8_t getA(SomeStruct *s) { return s->a; }

uint8_t getB(SomeStruct *s) { return s->b; }
#endif  // WRITESTRUCTTOFILE_H
