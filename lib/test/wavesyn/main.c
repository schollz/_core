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

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SAMPLE_RATE 44100
#define FREQUENCY 440
#define DURATION 6

#include "../../wavetablesyn.h"

void write_wav_header(FILE *file, int num_samples) {
  int bits_per_sample = 32;
  int byte_rate = SAMPLE_RATE * (bits_per_sample / 8);
  int block_align = bits_per_sample / 8;
  int subchunk2_size = num_samples * block_align;
  int chunk_size = 36 + subchunk2_size;
  int sample_rate = SAMPLE_RATE;

  fwrite("RIFF", 1, 4, file);
  fwrite(&chunk_size, 4, 1, file);
  fwrite("WAVE", 1, 4, file);
  fwrite("fmt ", 1, 4, file);

  int subchunk1_size = 16;
  int16_t audio_format = 1;
  int16_t num_channels = 1;

  fwrite(&subchunk1_size, 4, 1, file);
  fwrite(&audio_format, 2, 1, file);
  fwrite(&num_channels, 2, 1, file);
  fwrite(&sample_rate, 4, 1, file);
  fwrite(&byte_rate, 4, 1, file);
  fwrite(&block_align, 2, 1, file);
  fwrite(&bits_per_sample, 2, 1, file);

  fwrite("data", 1, 4, file);
  fwrite(&subchunk2_size, 4, 1, file);
}

int main() {
  int num_samples = DURATION * SAMPLE_RATE;
  int32_t sine_wave[num_samples];
  FILE *file = fopen("sine_wave.wav", "wb");
  if (!file) {
    printf("Error opening file\n");
    return 1;
  }

  WaveSyn *wavesyn = WaveSyn_malloc();
  // WaveSyn_new(wavesyn, 19, 3, 0, 0);
  // for (int i = 0; i < 441 * 4; i++) {
  //   printf("%d\n", WaveSyn_next(wavesyn));
  // }

  WaveSyn_new(wavesyn, 18, 1, 5, 12);
  int j = 0;
  for (int i = 0; i < 44100; i++) {
    sine_wave[j] = WaveSyn_next(wavesyn);
    printf("%d\n", sine_wave[j]);
    j++;
  }
  WaveSyn_new(wavesyn, 19, 1, 5, 12);
  for (int i = 0; i < 44100; i++) {
    sine_wave[j] = WaveSyn_next(wavesyn);
    printf("%d\n", sine_wave[j]);
    j++;
  }
  WaveSyn_new(wavesyn, 25, 1, 2, 12);
  for (int i = 0; i < 44100; i++) {
    sine_wave[j] = WaveSyn_next(wavesyn);
    printf("%d\n", sine_wave[j]);
    j++;
  }
  WaveSyn_new(wavesyn, 8, 1, 1, 12);
  for (int i = 0; i < 44100; i++) {
    sine_wave[j] = WaveSyn_next(wavesyn);
    printf("%d\n", sine_wave[j]);
    j++;
  }
  WaveSyn_new(wavesyn, 17, 1, 5, 12);
  for (int i = 0; i < 44100; i++) {
    sine_wave[j] = WaveSyn_next(wavesyn);
    printf("%d\n", sine_wave[j]);
    j++;
  }
  WaveSyn_release(wavesyn);
  for (int i = 0; i < 44100; i++) {
    sine_wave[j] = WaveSyn_next(wavesyn);
    printf("%d\n", sine_wave[j]);
    j++;
  }

  WaveSyn_free(wavesyn);

  // Write WAV file
  write_wav_header(file, num_samples);
  fwrite(sine_wave, sizeof(int32_t), num_samples, file);

  fclose(file);
  printf("WAV file created.\n");
  return 0;
}