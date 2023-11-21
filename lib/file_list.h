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

#ifndef INCLUDE_STEREO
#define NUM_AUDIO_CHANNELS 1
#endif
#ifdef INCLUDE_STEREO
#define NUM_AUDIO_CHANNELS 2
#endif

typedef struct SampleInfo {
  char *name;
  uint32_t size;
  uint16_t bpm;
  uint16_t beats;
  uint16_t slice_num;
  uint32_t *slice_start;
  uint32_t *slice_stop;
  uint8_t tempo_match;
  uint8_t stop_condition;
  uint8_t oversampling;
  uint8_t num_channels;

  // internal variables
  uint16_t slice_current;
} SampleInfo;

typedef struct Sample {
  SampleInfo *snd[FILE_VARIATIONS];
} Sample;

typedef struct SampleList {
  uint16_t num_samples;
  Sample *sample;
} SampleList;

SampleInfo *SampleInfo_load(const char *dir, char *fno) {
  char fname[100];
  strcpy(fname, dir);
  strcat(fname, "/");
  strcat(fname, fno);

  SampleInfo *si;
  FIL fil;
  FRESULT fr;
  fr = f_open(&fil, fname, FA_READ);
  if (fr != FR_OK) {
    printf("[sampleinfo] %s\n", FRESULT_str(fr));
  }
  unsigned int bytes_read;
  uint16_t sampleInfoSize;
  fr = f_read(&fil, &sampleInfoSize, sizeof(uint16_t), &bytes_read);
  if (fr != FR_OK || bytes_read == 0) {
    printf("[sampleinfo] %s, bytes read = %d\n", FRESULT_str(fr), bytes_read);
    return NULL;
  }
  si = malloc(sampleInfoSize);

  // read in the name len
  uint16_t NameLen;
  fr = f_read(&fil, &NameLen, sizeof(uint16_t), &bytes_read);
  if (fr != FR_OK) {
    printf("[sampleinfo] %s\n", FRESULT_str(fr));
  }

  // Name
  si->name = (char *)malloc((NameLen + 1) * sizeof(char));
  for (uint8_t i = 0; i < NameLen; i++) {
    fr = f_read(&fil, &si->name[i], sizeof(char), &bytes_read);
    if (fr != FR_OK) {
      printf("[sampleinfo] %s\n", FRESULT_str(fr));
    }
  }
  si->name[NameLen] = '\0';

  // Size
  fr = f_read(&fil, &si->size, sizeof(uint32_t), &bytes_read);
  if (fr != FR_OK) {
    printf("[sampleinfo] %s\n", FRESULT_str(fr));
  }

  // BPM
  fr = f_read(&fil, &si->bpm, sizeof(uint16_t), &bytes_read);
  if (fr != FR_OK) {
    printf("[sampleinfo] %s\n", FRESULT_str(fr));
  }

  // SliceNum
  fr = f_read(&fil, &si->slice_num, sizeof(uint16_t), &bytes_read);
  if (fr != FR_OK) {
    printf("[sampleinfo] %s\n", FRESULT_str(fr));
  }

  // SliceStart
  si->slice_start = (uint32_t *)malloc(si->slice_num * sizeof(uint32_t));
  for (uint8_t i = 0; i < si->slice_num; i++) {
    fr = f_read(&fil, &si->slice_start[i], sizeof(uint32_t), &bytes_read);
    if (fr != FR_OK) {
      printf("[sampleinfo] %s\n", FRESULT_str(fr));
    }
    // validate
    si->slice_start[i] = (si->slice_start[i] / 4) * 4;
  }

  // SliceStop
  si->slice_stop = (uint32_t *)malloc(si->slice_num * sizeof(uint32_t));
  for (uint8_t i = 0; i < si->slice_num; i++) {
    fr = f_read(&fil, &si->slice_stop[i], sizeof(uint32_t), &bytes_read);
    if (fr != FR_OK) {
      printf("[sampleinfo] %s\n", FRESULT_str(fr));
    }
    // validate
    si->slice_stop[i] = (si->slice_stop[i] / 4) * 4;
  }

  // BPMTransposable
  fr = f_read(&fil, &si->tempo_match, sizeof(uint8_t), &bytes_read);
  if (fr != FR_OK) {
    printf("[sampleinfo] %s\n", FRESULT_str(fr));
  }

  // StopCondition
  fr = f_read(&fil, &si->stop_condition, sizeof(uint8_t), &bytes_read);
  if (fr != FR_OK) {
    printf("[sampleinfo] %s\n", FRESULT_str(fr));
  }

  // Oversampling
  fr = f_read(&fil, &si->oversampling, sizeof(uint8_t), &bytes_read);
  if (fr != FR_OK) {
    printf("[sampleinfo] %s\n", FRESULT_str(fr));
  }

  // NumChannels
  fr = f_read(&fil, &si->num_channels, sizeof(uint8_t), &bytes_read);
  if (fr != FR_OK) {
    printf("[sampleinfo] %s\n", FRESULT_str(fr));
  }

  f_close(&fil);

  // internal
  si->slice_current = 0;

  return si;
}

uint8_t count_files(const char *dir, int num_channels) {
  uint8_t filelist_count = 0;
  DIR dj;      /* Directory object */
  FILINFO fno; /* File information */
  FRESULT fr;  /* File result error */

  memset(&dj, 0, sizeof dj);
  memset(&fno, 0, sizeof fno);
  fr = f_findfirst(&dj, &fno, dir, "*");
  if (FR_OK != fr) {
    printf("[file_list] f_findfirst error: %s (%d)\n", FRESULT_str(fr), fr);
    return filelist_count;
  }
  while (fr == FR_OK && fno.fname[0]) { /* Repeat while an item is found */
    if (filelist_count >= 16) {
      break;
    }
    if ((strstr(fno.fname, ".mono.wav.info") &&
         !strstr(fno.fname, ".mono.wav.info.") && NUM_AUDIO_CHANNELS == 1) ||
        (strstr(fno.fname, ".stereo.wav.info") &&
         !strstr(fno.fname, ".stereo.wav.info.") && NUM_AUDIO_CHANNELS == 2)) {
      filelist_count++;
    }
    fr = f_findnext(&dj, &fno); /* Search for next item */
  }
  f_closedir(&dj);

  return filelist_count;
}

SampleList *list_files(const char *dir, int num_channels) {
  DIR dj;      /* Directory object */
  FILINFO fno; /* File information */
  FRESULT fr;  /* File result error */
  uint8_t total_files = count_files(dir, num_channels);
  SampleList *samplelist = malloc(sizeof(SampleList));
  samplelist->num_samples = total_files;
  if (total_files == 0) {
    printf("%s: %d\n", dir, total_files);
    samplelist->sample == NULL;
    return samplelist;
  }
  samplelist->sample = malloc(sizeof(Sample) * total_files);

  memset(&dj, 0, sizeof dj);
  memset(&fno, 0, sizeof fno);
  fr = f_findfirst(&dj, &fno, dir, "*");
  if (FR_OK != fr) {
    printf("[file_list] f_findfirst error: %s (%d)\n", FRESULT_str(fr), fr);
    return samplelist;
  }
  uint8_t filelist_count = 0;
  while (fr == FR_OK && fno.fname[0]) { /* Repeat while an item is found */
    if (filelist_count >= 16) {
      break;
    }
    if ((strstr(fno.fname, ".mono.wav.info") &&
         !strstr(fno.fname, ".mono.wav.info.") && NUM_AUDIO_CHANNELS == 1) ||
        (strstr(fno.fname, ".stereo.wav.info") &&
         !strstr(fno.fname, ".stereo.wav.info.") && NUM_AUDIO_CHANNELS == 2)) {
      samplelist->sample[filelist_count].snd[0] =
          SampleInfo_load(dir, fno.fname);
      for (uint8_t k = 1; k < FILE_VARIATIONS; k++) {
        char fname2[100];
        sprintf(fname2, "%s.%d", fno.fname, k);
        samplelist->sample[filelist_count].snd[k] =
            SampleInfo_load(dir, fname2);
      }
      filelist_count++;
    }
    fr = f_findnext(&dj, &fno); /* Search for next item */
  }
  f_closedir(&dj);

  return samplelist;
}
