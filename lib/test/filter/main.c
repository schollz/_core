#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define FILTER_LOWPASS 0
#define FILTER_BANDPASS 1
#define FILTER_HIGHPASS 2
#define FILTER_NOTCH 3

#define Q_BITS 8
#define FRACTIONAL_BITS (1 << Q_BITS)

// Function to convert int16_t to Q8.10 fixed-point
int32_t int16ToFixedPoint(int16_t value) { return (int32_t)value << Q_BITS; }

int32_t floatToFixedPoint(float value) {
  return (int32_t)(value * FRACTIONAL_BITS);
}
// Function to convert Q8.10 fixed-point to int16_t
int16_t fixedPointToInt16(int32_t fixedValue) {
  return (int16_t)(fixedValue >> Q_BITS);
}

// Function to convert a Q8.10 fixed-point value to a float
float fixedPointToFloat(int32_t value) {
  return (float)value / FRACTIONAL_BITS;
}

int32_t multiplyFixedPoint(int32_t a, int32_t b) {
  int32_t result = (int32_t)(((int64_t)a * b) >> Q_BITS);
  return result;
}

// Function to divide two Q8.10 fixed-point numbers
int32_t divideFixedPoint(int32_t numerator, int32_t denominator) {
  // Ensure that the denominator is not zero to avoid division by zero
  if (denominator == 0) {
    // Handle division by zero
    // You can choose to return an error code or a special value
    return INT32_MAX;  // For example, return the maximum value
  }

  // Perform fixed-point division
  int32_t result = (int32_t)(((int64_t)numerator << Q_BITS) / denominator);
  return result;
}

typedef struct ResonantFilter {
  int32_t f;
  int32_t q;
  int32_t fb;
  int32_t buf0;
  int32_t buf1;
  uint8_t filter_type;
} ResonantFilter;

ResonantFilter* ResonantFilter_create(float f, float q, uint8_t filter_type) {
  ResonantFilter* rf;
  rf = (ResonantFilter*)malloc(sizeof(ResonantFilter));
  rf->f = floatToFixedPoint(2.0 * sin(3.14159265358979 * 4000 / 44100));
  rf->q = floatToFixedPoint(q);
  rf->filter_type = filter_type;
  rf->buf0 = floatToFixedPoint(0.0);
  rf->buf1 = floatToFixedPoint(0.0);
  rf->fb = rf->q + divideFixedPoint(rf->q, (floatToFixedPoint(1.0) - rf->f));
  return rf;
}

int16_t ResonantFilter_update(ResonantFilter* rf, int16_t in) {
  int32_t inFixed = int16ToFixedPoint(in);
  rf->buf0 += multiplyFixedPoint(
      rf->f,
      (inFixed - rf->buf0 + multiplyFixedPoint(rf->fb, rf->buf0 - rf->buf1)));
  rf->buf1 += multiplyFixedPoint(rf->f, rf->buf0 - rf->buf1);
  return fixedPointToInt16(rf->buf1);
}

int main() {
  // Input and output file names
  const char* inputFileName = "amen_0efedaab_beats8_bpm165.mono.wav";
  const char* outputFileName = "out.wav";

  FILE *inputFile, *outputFile;

  // Open the input file in binary read mode
  inputFile = fopen(inputFileName, "rb");
  if (inputFile == NULL) {
    perror("Error opening input file");
    return 1;
  }

  // Open the output file in binary write mode
  outputFile = fopen(outputFileName, "wb");
  if (outputFile == NULL) {
    perror("Error opening output file");
    fclose(inputFile);
    return 1;
  }

  // Read and write the same header
  for (uint8_t i = 0; i < 44; i++) {
    char num;
    fread(&num, sizeof(char), 1, inputFile);
    fwrite(&num, sizeof(char), 1, outputFile);
  }

  // create resonsnat filter
  ResonantFilter* rf = ResonantFilter_create(0.01, 0.2, 0);

  clock_t start, end;
  double cpu_time_used;
  start = clock();
  // Read and write 16-bit integers from the input to the output file
  int16_t number;
  while (fread(&number, sizeof(int16_t), 1, inputFile) == 1) {
    number = ResonantFilter_update(rf, number);
    // fwrite(&number, sizeof(int16_t), 1, outputFile);
  }
  end = clock();
  cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC;
  printf("cpu_time_used: %f\n", cpu_time_used);

  // Close the files
  fclose(inputFile);
  fclose(outputFile);

  return 0;
}
