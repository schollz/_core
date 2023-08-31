#include <stdint.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
  if (argc != 2) {
    printf("Usage: %s <filename>\n", argv[0]);
    return 1;
  }

  // Get the file name from the argument.
  char *filename = argv[1];

  // Declare the file pointer.
  FILE *fp;

  // Open the file.
  fp = fopen(filename, "rb");
  if (fp == NULL) {
    printf("Error opening file.\n");
    return 1;
  }

  // Read the integers from the file.
  int16_t integer;
  int i = 0;
  while (fread(&integer, sizeof(int16_t), 1, fp) == 1) {
    printf("%d,", integer);
    i++;
    if (i > 0 && i % 20 == 0) {
      printf("\n\t");
    }
  }

  // Close the file.
  fclose(fp);

  return 0;
}
