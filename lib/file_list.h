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

typedef struct SampleInfo {
  int32_t size;
  uint16_t bpm;
  uint16_t beats;
  uint16_t slice_num;
  int32_t *slice_start;
  int32_t *slice_stop;
  uint8_t tempo_match;
  uint8_t play_mode;
  uint16_t splice_trigger;
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

SampleInfo *SampleInfo_load(const char *fname) {
  SampleInfo *si;
  FIL fil;
  FRESULT fr;
  fr = f_open(&fil, fname, FA_READ);
  if (fr != FR_OK) {
    printf("[sampleinfo] %s\n", FRESULT_str(fr));
  }
  unsigned int bytes_read;

  // total size to allocate
  uint16_t sampleInfoSize;
  fr = f_read(&fil, &sampleInfoSize, sizeof(uint16_t), &bytes_read);
  if (fr != FR_OK || bytes_read == 0) {
    printf("[sampleinfo] %s, bytes read = %d\n", FRESULT_str(fr), bytes_read);
    return NULL;
  }
  si = malloc(sampleInfoSize);

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

  // play_mode
  fr = f_read(&fil, &si->play_mode, sizeof(uint8_t), &bytes_read);
  if (fr != FR_OK) {
    printf("[sampleinfo] %s\n", FRESULT_str(fr));
  }

  // Splice trigger
  fr = f_read(&fil, &si->splice_trigger, sizeof(uint16_t), &bytes_read);
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

SampleList *list_files(const char *dir, int num_channels) {
  uint8_t total_files = count_files(dir, num_channels);
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
    if (FR_OK == fr) {
      for (uint8_t j = 0; j < FILE_VARIATIONS; j++) {
        char fnameLoad[100];
        sprintf(fnameLoad, "%s/%d.%d.wav.info", dir, i, j);
        samplelist->sample[filelist_count].snd[j] = SampleInfo_load(fnameLoad);
      }
      filelist_count++;
    }
  }

  return samplelist;
}
