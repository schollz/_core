#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// Define a struct to hold the data
typedef struct MyStruct {
  uint32_t size;
  uint32_t a;
  uint32_t b;
  uint8_t c;
  int8_t d;
  int8_t e_size;
  uint32_t* e;
  int16_t f_size;
  char* f;
} MyStruct;

int main() {
  FILE* file = fopen("test", "rb");  // Replace "data.dat" with your file name
  if (file == NULL) {
    perror("Error opening file");
    return 1;
  }

  // Read the size of the struct from the file
  uint32_t structSize;
  if (fread(&structSize, sizeof(uint32_t), 1, file) != 1) {
    perror("Error reading struct size");
    fclose(file);
    return 1;
  }

  // Allocate memory for the struct
  MyStruct* myData = (MyStruct*)malloc(structSize);
  if (myData == NULL) {
    perror("Memory allocation failed");
    fclose(file);
    return 1;
  }

  myData->size = structSize;
  fread(&myData->a, sizeof(uint32_t), 1, file);
  fread(&myData->b, sizeof(uint32_t), 1, file);
  fread(&myData->c, sizeof(uint8_t), 1, file);
  fread(&myData->d, sizeof(int8_t), 1, file);
  fread(&myData->e_size, sizeof(int8_t), 1, file);
  printf("myData->e_size: %d\n", myData->e_size);
  myData->e = (uint32_t*)malloc(myData->e_size * sizeof(uint32_t));
  for (uint8_t i = 0; i < myData->e_size; i++) {
    fread(&myData->e[i], sizeof(uint32_t), 1, file);
  }

  fread(&myData->f_size, sizeof(int16_t), 1, file);
  printf("myData->f_size: %d\n", myData->f_size);
  myData->f = (char*)malloc(myData->f_size * sizeof(char));
  for (uint8_t i = 0; i < myData->f_size; i++) {
    fread(&myData->f[i], sizeof(char), 1, file);
  }

  // Close the file
  fclose(file);
  printf("myData->size: %d\n", myData->size);
  printf("myData->a: %d\n", myData->a);
  printf("myData->b: %d\n", myData->b);
  printf("myData->c: %d\n", myData->c);
  printf("myData->d: %d\n", myData->d);
  for (uint8_t i = 0; i < myData->e_size; i++) {
    printf("myData->e[%d]: %d\n", i, myData->e[i]);
  }
  printf("myData->f: %s\n", myData->f);

  free(myData->e);
  free(myData->f);
  free(myData);

  return 0;
}