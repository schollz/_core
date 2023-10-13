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
#include <stdlib.h>

// Define a struct to hold the data
typedef struct MyStruct {
  uint32_t size;
  uint32_t a;
  uint32_t b;
  uint8_t c;
  int8_t d;
  int8_t e_size;
  int8_t* e;
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

  myData->e = (int8_t*)malloc(myData->e_size);
  for (uint8_t i = 0; i < myData->e_size; i++) {
    fread(&myData->e[i], sizeof(int8_t), 1, file);
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
  free(myData);

  return 0;
}