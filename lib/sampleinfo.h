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

#ifndef SAMPLEINFO_H
#define SAMPLEINFO_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct SampleInfo {
  uint32_t size;
  // flags
  uint16_t bpm : 9;           // 0-511
  uint16_t play_mode : 3;     // 0-7
  uint16_t one_shot : 1;      // 0-1 (off/on)
  uint16_t tempo_match : 1;   // 0-1 (off/on)
  uint16_t oversampling : 1;  // 0-1 (1x or 2x)
  uint16_t num_channels : 1;  // 0-1 (mono or stereo)
  // splice_info
  uint16_t splice_trigger : 15;
  uint16_t splice_variable : 1;  // 0-1 (off/on)
  uint8_t slice_num;             // 0-255
  uint8_t slice_current;         // 0-255
  int32_t *slice_start;
  int32_t *slice_stop;
  int8_t *slice_type;
} SampleInfo;

typedef struct SampleInfoPack {
  uint32_t size;
  uint32_t flags;  // Holds bpm, slice_num, slice_current, play_mode, one_shot,
                   // tempo_match, oversampling, and num_channels
  uint16_t splice_info;   // Holds splice_trigger and splice_variable
  uint8_t slice_num;      // 0-255
  uint8_t slice_current;  // 0-255
  int32_t *slice_start;
  int32_t *slice_stop;
  int8_t *slice_type;
} SampleInfoPack;

SampleInfoPack *SampleInfo_Marshal(SampleInfo *self) {
  SampleInfoPack *pack = (SampleInfoPack *)malloc(sizeof(SampleInfoPack));
  if (pack == NULL) {
    // Handle memory allocation failure if necessary
    return NULL;
  }

  pack->size = self->size;
  pack->flags = (self->bpm & 0x1FF) | ((uint16_t)self->play_mode << 9) |
                ((uint16_t)self->one_shot << 12) |
                ((uint16_t)self->tempo_match << 13) |
                ((uint16_t)self->oversampling << 14) |
                ((uint16_t)self->num_channels << 15);
  pack->splice_info =
      (self->splice_trigger & 0x7FFF) | ((uint16_t)self->splice_variable << 15);
  pack->slice_num = self->slice_num;
  pack->slice_current = self->slice_current;

  pack->slice_start = malloc(sizeof(int32_t) * self->slice_num);
  if (pack->slice_start == NULL) {
    perror("Error allocating memory for array");
    free(pack);
    return NULL;
  }
  memcpy(pack->slice_start, self->slice_start,
         sizeof(int32_t) * self->slice_num);

  pack->slice_stop = malloc(sizeof(int32_t) * self->slice_num);
  if (pack->slice_stop == NULL) {
    perror("Error allocating memory for array");
    free(pack->slice_start);
    free(pack);
    return NULL;
  }
  memcpy(pack->slice_stop, self->slice_stop, sizeof(int32_t) * self->slice_num);

  pack->slice_type = malloc(sizeof(int8_t) * self->slice_num);
  if (pack->slice_type == NULL) {
    perror("Error allocating memory for array");
    free(pack->slice_start);
    free(pack->slice_stop);
    free(pack);
    return NULL;
  }
  memcpy(pack->slice_type, self->slice_type, sizeof(int8_t) * self->slice_num);

  return pack;
}

SampleInfo *SampleInfo_Unmarshal(SampleInfoPack *pack) {
  SampleInfo *si = (SampleInfo *)malloc(sizeof(SampleInfo));
  if (si == NULL) {
    // Handle memory allocation failure if necessary
    return NULL;
  }

  si->size = pack->size;
  si->bpm = pack->flags & 0x1FF;
  si->play_mode = (pack->flags >> 9) & 0x7;
  si->one_shot = (pack->flags >> 12) & 0x1;
  si->tempo_match = (pack->flags >> 13) & 0x1;
  si->oversampling = (pack->flags >> 14) & 0x1;
  si->num_channels = (pack->flags >> 15) & 0x1;
  si->slice_num = pack->slice_num;
  si->slice_current = pack->slice_current;
  si->splice_trigger = pack->splice_info & 0x7FFF;
  si->splice_variable = (pack->splice_info >> 15) & 0x1;

  // copy pointer into new pointer
  si->slice_start = malloc(sizeof(int32_t) * si->slice_num);
  if (si->slice_start == NULL) {
    perror("Error allocating memory for array");
    free(si);
    return NULL;
  }
  memcpy(si->slice_start, pack->slice_start, sizeof(int32_t) * si->slice_num);
  si->slice_stop = malloc(sizeof(int32_t) * si->slice_num);
  if (si->slice_stop == NULL) {
    perror("Error allocating memory for array");
    free(si->slice_start);
    free(si);
    return NULL;
  }
  memcpy(si->slice_stop, pack->slice_stop, sizeof(int32_t) * si->slice_num);

  si->slice_type = malloc(sizeof(int8_t) * si->slice_num);
  if (si->slice_type == NULL) {
    perror("Error allocating memory for array");
    free(si->slice_start);
    free(si->slice_stop);
    free(si);
    return NULL;
  }
  memcpy(si->slice_type, pack->slice_type, sizeof(int8_t) * si->slice_num);

  return si;
}

void SampleInfoPack_writeToFile(SampleInfoPack *self, const char *filename) {
  FILE *file = fopen(filename, "wb");
  if (file == NULL) {
    perror("Error opening file");
    return;
  }

  // Write the struct (excluding the arrays)
  if (fwrite(
          self,
          sizeof(SampleInfoPack) - (2 * sizeof(int32_t *) - sizeof(int8_t *)),
          1, file) != 1) {
    perror("Error writing struct to file");
    fclose(file);
    return;
  }

  // Write the array content
  if (fwrite(self->slice_start, sizeof(int32_t), self->slice_num, file) !=
      self->slice_num) {
    perror("Error writing array to file");
    fclose(file);
    return;
  }

  // Write the array content
  if (fwrite(self->slice_stop, sizeof(int32_t), self->slice_num, file) !=
      self->slice_num) {
    perror("Error writing array to file");
    fclose(file);
    return;
  }

  // Write the array content
  if (fwrite(self->slice_type, sizeof(int8_t), self->slice_num, file) !=
      self->slice_num) {
    perror("Error writing array to file");
    fclose(file);
    return;
  }

  fclose(file);
}

SampleInfoPack *SampleInfoPack_readFromFile(const char *filename) {
  FILE *file = fopen(filename, "rb");
  if (file == NULL) {
    perror("Error opening file");
    return NULL;
  }

  SampleInfoPack *pack = (SampleInfoPack *)malloc(sizeof(SampleInfoPack));
  if (pack == NULL) {
    perror("Error allocating memory");
    fclose(file);
    return NULL;
  }

  if (fread(pack, 12, 1, file) != 1) {
    perror("Error reading struct from file");
    fclose(file);
    free(pack);
    return NULL;
  }

  fprintf(stderr, "pack->slice_num = %d\n", pack->slice_num);

  // Allocate memory for the array
  pack->slice_start = (int32_t *)malloc(sizeof(int32_t) * pack->slice_num);
  if (pack->slice_start == NULL) {
    perror("Error allocating memory for array");
    fclose(file);
    free(pack);
    return NULL;
  }
  // Read the array content
  int bytes_read =
      fread(pack->slice_start, sizeof(int32_t), pack->slice_num, file);
  if (bytes_read != pack->slice_num) {
    fprintf(stderr, "bytes_read = %d\n", bytes_read);
    perror("Error reading slice_start from file");
    fclose(file);
    free(pack->slice_start);
    free(pack);
    return NULL;
  }

  // Allocate memory for the slice_stop array
  pack->slice_stop = (int32_t *)malloc(sizeof(int32_t) * pack->slice_num);
  if (pack->slice_stop == NULL) {
    perror("Error allocating memory for slice_stop array");
    fclose(file);
    free(pack->slice_start);
    free(pack);
    return NULL;
  }
  // Read the slice_stop array content
  if (fread(pack->slice_stop, sizeof(int32_t), pack->slice_num, file) !=
      pack->slice_num) {
    perror("Error reading slice_stop array from file");
    fclose(file);
    free(pack->slice_start);
    free(pack->slice_stop);
    free(pack);
    return NULL;
  }

  // Allocate memory for the slice_type array
  pack->slice_type = (int8_t *)malloc(sizeof(int8_t) * pack->slice_num);
  if (pack->slice_type == NULL) {
    perror("Error allocating memory for slice_stop array");
    fclose(file);
    free(pack->slice_start);
    free(pack->slice_stop);
    free(pack);
    return NULL;
  }
  // Read the slice_type array content
  if (fread(pack->slice_type, sizeof(int8_t), pack->slice_num, file) !=
      pack->slice_num) {
    perror("Error reading slice_stop array from file");
    fclose(file);
    free(pack->slice_start);
    free(pack->slice_stop);
    free(pack->slice_type);
    free(pack);
    return NULL;
  }

  fclose(file);
  return pack;
}

void SampleInfoPack_free(SampleInfoPack *self) {
  if (self != NULL) {
    free(self->slice_start);
    free(self->slice_stop);
    free(self->slice_type);
  }
  free(self);
}

void SampleInfo_free(SampleInfo *self) {
  if (self != NULL) {
    free(self->slice_start);
    free(self->slice_stop);
    free(self->slice_type);
  }
  free(self);
}

SampleInfo *SampleInfo_malloc(uint32_t size, uint32_t bpm, uint8_t play_mode,
                              uint8_t one_shot, uint16_t splice_trigger,
                              uint8_t splice_variable, uint8_t tempo_match,
                              uint8_t oversampling, uint8_t num_channels,
                              uint32_t slice_num, int32_t *slice_start,
                              int32_t *slice_stop, int8_t *slice_type) {
  SampleInfo *si = (SampleInfo *)malloc(sizeof(SampleInfo));
  if (si == NULL) {
    perror("Error allocating memory for struct");
    return NULL;
  }
  fprintf(stderr, "sizeof(SampleInfo) = %lu\n", sizeof(SampleInfo));
  si->size = size;
  si->bpm = bpm;
  si->slice_num = slice_num;
  si->slice_current = 0;
  si->play_mode = play_mode;
  si->one_shot = one_shot;
  si->splice_trigger = splice_trigger;
  fprintf(stderr, "splice_trigger = %d\n", splice_trigger);
  si->splice_variable = splice_variable;
  si->tempo_match = tempo_match;
  si->oversampling = oversampling;
  si->num_channels = num_channels;

  si->slice_start = malloc(sizeof(int32_t) * si->slice_num);
  if (si->slice_start == NULL) {
    perror("Error allocating memory for array");
    free(si);
    return NULL;
  }
  for (int i = 0; i < si->slice_num; i++) {
    si->slice_start[i] = slice_start[i];
  }

  si->slice_stop = malloc(sizeof(int32_t) * si->slice_num);
  if (si->slice_stop == NULL) {
    perror("Error allocating memory for array");
    free(si->slice_start);
    free(si);
    return NULL;
  }
  for (int i = 0; i < si->slice_num; i++) {
    si->slice_stop[i] = slice_stop[i];
  }

  si->slice_type = malloc(sizeof(int8_t) * si->slice_num);
  if (si->slice_type == NULL) {
    perror("Error allocating memory for array");
    free(si->slice_start);
    free(si->slice_stop);
    free(si);
    return NULL;
  }
  for (int i = 0; i < si->slice_num; i++) {
    si->slice_type[i] = slice_type[i];
  }

  return si;
}

uint16_t SampleInfo_getBPM(SampleInfo *si) { return si->bpm; }

uint16_t SampleInfo_getSliceNum(SampleInfo *si) { return si->slice_num; }

int32_t SampleInfo_getSliceStop(SampleInfo *si, uint16_t i) {
  return si->slice_stop[i];
}

uint16_t SampleInfo_getSpliceTrigger(SampleInfo *si) {
  return si->splice_trigger;
}

int32_t SampleInfo_getSliceStart(SampleInfo *si, uint16_t i) {
  return si->slice_start[i];
}

uint8_t SampleInfo_getSliceType(SampleInfo *si, uint16_t i) {
  return si->slice_type[i];
}

uint8_t SampleInfo_getSpliceVariable(SampleInfo *si) {
  return si->splice_variable;
}

uint8_t SampleInfo_getNumChannels(SampleInfo *si) { return si->num_channels; }

int SampleInfo_writeToDisk(SampleInfo *si) {
  FILE *file = fopen("sampleinfo.bin", "wb");
  if (file == NULL) {
    perror("Error opening file");
    SampleInfo_free(si);
    return -1;
  }

  // Write the struct (excluding the arrays)
  if (fwrite(si, sizeof(SampleInfo) - (2 * sizeof(int32_t *)), 1, file) != 1) {
    perror("Error writing struct to file");
    fclose(file);
    SampleInfo_free(si);
    return -1;
  }

  // Write the array content
  if (fwrite(si->slice_start, sizeof(int32_t), si->slice_num, file) !=
      si->slice_num) {
    perror("Error writing array to file");
    fclose(file);
    SampleInfo_free(si);
    return -1;
  }

  // Write the array content
  if (fwrite(si->slice_stop, sizeof(int32_t), si->slice_num, file) !=
      si->slice_num) {
    perror("Error writing array to file");
    fclose(file);
    SampleInfo_free(si);
    return -1;
  }

  // Write the array content
  if (fwrite(si->slice_type, sizeof(int8_t), si->slice_num, file) !=
      si->slice_num) {
    perror("Error writing array to file");
    fclose(file);
    SampleInfo_free(si);
    return -1;
  }

  fclose(file);
  return 0;
}

SampleInfo *SampleInfo_readFromDisk() {
  FILE *file = fopen("sampleinfo.bin", "rb");
  if (file == NULL) {
    perror("Error opening file");
    return NULL;
  }

  SampleInfo *si = (SampleInfo *)malloc(sizeof(SampleInfo));
  if (si == NULL) {
    perror("Error allocating memory");
    fclose(file);
    return NULL;
  }

  if (fread(si, sizeof(SampleInfo) - (2 * sizeof(int32_t *)), 1, file) != 1) {
    perror("Error reading struct from file");
    fclose(file);
    SampleInfo_free(si);
    return NULL;
  }

  // Allocate memory for the array
  si->slice_start = (int32_t *)malloc(sizeof(int32_t) * si->slice_num);
  if (si->slice_start == NULL) {
    perror("Error allocating memory for array");
    fclose(file);
    SampleInfo_free(si);
    return NULL;
  }
  // Read the array content
  if (fread(si->slice_start, sizeof(int32_t), si->slice_num, file) !=
      si->slice_num) {
    perror("Error reading array from file");
    fclose(file);
    SampleInfo_free(si);
    return NULL;
  }

  // Allocate memory for the slice_stop array
  si->slice_stop = (int32_t *)malloc(sizeof(int32_t) * si->slice_num);
  if (si->slice_stop == NULL) {
    perror("Error allocating memory for slice_stop array");
    fclose(file);
    free(si->slice_start);
    SampleInfo_free(si);
    return NULL;
  }
  // Read the slice_stop array content
  if (fread(si->slice_stop, sizeof(int32_t), si->slice_num, file) !=
      si->slice_num) {
    perror("Error reading slice_stop array from file");
    fclose(file);
    free(si->slice_start);
    free(si->slice_stop);
    SampleInfo_free(si);
    return NULL;
  }

  // Allocate memory for the slice_type array
  si->slice_type = (int8_t *)malloc(sizeof(int8_t) * si->slice_num);
  if (si->slice_type == NULL) {
    perror("Error allocating memory for slice_stop array");
    fclose(file);
    free(si->slice_start);
    free(si->slice_stop);
    SampleInfo_free(si);
    return NULL;
  }
  // Read the slice_type array content
  if (fread(si->slice_type, sizeof(int8_t), si->slice_num, file) !=
      si->slice_num) {
    perror("Error reading slice_stop array from file");
    fclose(file);
    free(si->slice_start);
    free(si->slice_stop);
    free(si->slice_type);
    SampleInfo_free(si);
    return NULL;
  }

  fclose(file);

  // fix any problems
  if (si->bpm > 300) {
    si->bpm = 120;
  } else if (si->bpm < 30) {
    si->bpm = 120;
  }
  return si;
}

#endif