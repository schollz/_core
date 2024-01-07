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

#include "sampleinfo.h"

typedef struct Sample {
  SampleInfo *snd[FILE_VARIATIONS];
} Sample;

typedef struct SampleList {
  uint16_t num_samples;
  Sample *sample;
} SampleList;

SampleInfo *SampleInfo_load(const char *fname) {
  SampleInfo *si;
  FIL fil;
  FRESULT fr;
  fr = f_open(&fil, fname, FA_READ);
  if (fr != FR_OK) {
    printf("[sampleinfo] %s\n", FRESULT_str(fr));
  }
  unsigned int bytes_read;

  si = (SampleInfo *)malloc(sizeof(SampleInfo));

  // Size
  fr = f_read(
      &fil, si,
      sizeof(SampleInfo) - (2 * sizeof(int32_t *) - (1 * sizeof(uint8_t *))),
      &bytes_read);
  if (fr != FR_OK) {
    printf("[sampleinfo] %s\n", FRESULT_str(fr));
  }

  // Slice start
  si->slice_start = malloc(sizeof(int32_t) * si->slice_num);
  if (si->slice_start == NULL) {
    perror("Error allocating memory for array");
    free(si);
    return NULL;
  }
  fr = f_read(&fil, si->slice_start, sizeof(int32_t) * si->slice_num,
              &bytes_read);
  if (fr != FR_OK) {
    printf("[sampleinfo] %s\n", FRESULT_str(fr));
  }

  // Slice stop
  si->slice_stop = malloc(sizeof(int32_t) * si->slice_num);
  if (si->slice_stop == NULL) {
    perror("Error allocating memory for array");
    free(si->slice_start);
    return NULL;
  }
  fr = f_read(&fil, si->slice_stop, sizeof(int32_t) * si->slice_num,
              &bytes_read);
  if (fr != FR_OK) {
    printf("[sampleinfo] %s\n", FRESULT_str(fr));
  }

  // Slice type
  si->slice_type = malloc(sizeof(uint8_t) * si->slice_num);
  if (si->slice_type == NULL) {
    perror("Error allocating memory for array");
    free(si->slice_start);
    free(si->slice_stop);
    return NULL;
  }
  fr = f_read(&fil, si->slice_type, sizeof(uint8_t) * si->slice_num,
              &bytes_read);
  if (fr != FR_OK) {
    printf("[sampleinfo] %s\n", FRESULT_str(fr));
  }

  f_close(&fil);

  // internal
  si->slice_current = 0;

  return si;
}

uint8_t count_files(const char *dir) {
  uint8_t filelist_count = 0;
  FILINFO fno;

  for (uint8_t i = 0; i < 16; i++) {
    char fname[100];
    sprintf(fname, "%s/%d.0.wav.info", dir, i);
    FRESULT fr = f_stat(fname, &fno);
    if (FR_OK == fr) {
      filelist_count++;
    }
  }

  return filelist_count;
}

SampleList *list_files(const char *dir) {
  uint8_t total_files = count_files(dir);
  SampleList *samplelist = malloc(sizeof(SampleList));
  samplelist->num_samples = total_files;
  if (total_files == 0) {
    printf("%s: %d\n", dir, total_files);
    samplelist->sample == NULL;
    return samplelist;
  }
  samplelist->sample = malloc(sizeof(Sample) * total_files);

  uint8_t filelist_count = 0;
  for (uint8_t i = 0; i < 16; i++) {
    char fname[100];
    sprintf(fname, "%s/%d.0.wav.info", dir, i);
    FILINFO fno; /* File information */
    FRESULT fr = f_stat(fname, &fno);
    // printf("[list_files] %s, %s\n", fname, FRESULT_str(fr));
    if (FR_OK == fr) {
      for (uint8_t j = 0; j < FILE_VARIATIONS; j++) {
        char fnameLoad[100];
        sprintf(fnameLoad, "%s/%d.%d.wav.info", dir, i, j);
        samplelist->sample[filelist_count].snd[j] = SampleInfo_load(fnameLoad);
      }
      filelist_count++;
    } else {
      printf("[list_files] %s\n", FRESULT_str(fr));
    }
  }

  return samplelist;
}
