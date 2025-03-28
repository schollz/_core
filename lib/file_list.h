// Copyright 2023-2025 Zack Scholl, GPLv3.0

#include "sampleinfo.h"

typedef struct Sample {
  SampleInfo *snd[FILE_VARIATIONS];
} Sample;

typedef struct SampleList {
  uint16_t num_samples;
  Sample *sample;
} SampleList;

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
    // printf("%s: %d\n", dir, total_files);
    samplelist->sample = NULL;
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
        samplelist->sample[filelist_count].snd[j] =
            SampleInfo_load(fnameLoad, j == 0);
        break;  // Don't load all variations
      }
      filelist_count++;
    } else {
      // printf("[list_files] %s: %s\n", fname, FRESULT_str(fr));
    }
  }

  return samplelist;
}
