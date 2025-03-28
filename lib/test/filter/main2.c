#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define FILTER_LOWPASS 0
#define FILTER_BANDPASS 1
#define FILTER_HIGHPASS 2
#define FILTER_NOTCH 3

#define Q_BITS 16
#define FRACTIONAL_BITS (1 << Q_BITS)
#define PI_FLOAT 3.14159265358979

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
  return (int32_t)(((int64_t)a * b) >> Q_BITS);
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
  int32_t b0;
  int32_t b1;
  int32_t b2;
  int32_t a1;
  int32_t a2;
  int32_t x1_f;
  int32_t x2_f;
  int32_t y1_f;
  int32_t y2_f;
} ResonantFilter;

void ResonantFilter_reset(ResonantFilter* rf, float fc, float fs, float q,
                          float db, uint8_t filter_type) {
  float w0 = 2 * PI_FLOAT * (fc / fs);
  float cosW = cos(w0);
  float sinW = sin(w0);
  float A = pow(10, db / 40);
  float alpha = sinW / (2 * q);
  float beta = pow(A, 0.5) / q;
  float b1 = 1 - cosW;
  float b0 = b1 / 2;
  float b2 = b0;
  float a0 = 1 + alpha;
  float a1 = -2 * cosW;
  float a2 = 1 - alpha;
  b0 = b0 / a0;
  b1 = b1 / a0;
  b2 = b2 / a0;
  a1 = a1 / a0;
  a2 = a2 / a0;

  rf->b0 = floatToFixedPoint(b0);
  rf->b1 = floatToFixedPoint(b1);
  rf->b2 = floatToFixedPoint(b2);
  rf->a1 = floatToFixedPoint(a1);
  rf->a2 = floatToFixedPoint(a2);
}

ResonantFilter* ResonantFilter_create(float fc, float fs, float q, float db,
                                      uint8_t filter_type) {
  ResonantFilter* rf;
  rf = (ResonantFilter*)malloc(sizeof(ResonantFilter));
  ResonantFilter_reset(rf, fc, fs, q, db, filter_type);
  return rf;
}

int16_t ResonantFilter_update(ResonantFilter* rf, int16_t in) {
  int32_t x = int16ToFixedPoint(in);
  int32_t y = multiplyFixedPoint(rf->b0, x) +
              multiplyFixedPoint(rf->b1, rf->x1_f) +
              multiplyFixedPoint(rf->b2, rf->x2_f) -
              multiplyFixedPoint(rf->a1, rf->y1_f) -
              multiplyFixedPoint(rf->a2, rf->y2_f);

  rf->x2_f = rf->x1_f;
  rf->x1_f = x;
  rf->y2_f = rf->y1_f;
  rf->y1_f = y;
  return fixedPointToInt16(y);
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
  ResonantFilter* rf =
      ResonantFilter_create(400, 44100, 1 * 0.707, 0, FILTER_LOWPASS);

  clock_t start, end;
  double cpu_time_used;
  start = clock();
  // Read and write 16-bit integers from the input to the output file
  int16_t number;
  while (fread(&number, sizeof(int16_t), 1, inputFile) == 1) {
    number = ResonantFilter_update(rf, number);
    fwrite(&number, sizeof(int16_t), 1, outputFile);
  }
  end = clock();
  cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC;
  printf("cpu_time_used: %f\n", cpu_time_used);

  // Close the files
  fclose(inputFile);
  fclose(outputFile);

  return 0;
}
