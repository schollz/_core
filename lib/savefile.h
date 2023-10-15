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

typedef struct SaveFile {
  uint8_t chain_length;
  uint8_t chain_sequence[128];
  bool pattern_on;
  uint8_t pattern_current;
  uint8_t pattern_length[16];
  uint8_t pattern_sequence[16][128];
  uint8_t fx_length[16];
  uint8_t fx_sequence[16][128];
  uint16_t bpm_tempo;
  uint8_t vol;
  uint8_t distortion_level;
  uint8_t distortion_wet;
  uint8_t saturate_wet;
  uint8_t wavefold;
} SaveFile;

#define SAVEFILE_PATHNAME "save.bin"

SaveFile *SaveFile_New() {
  SaveFile *sf;
  sf = malloc(sizeof(SaveFile));
  sf->vol = 20;
  sf->bpm_tempo = 165;
  sf->chain_length = 0;
  sf->distortion_level = 0;
  sf->distortion_wet = 0;
  sf->saturate_wet = 0;
  sf->wavefold = 0;
  for (uint8_t i = 0; i < 128; i++) {
    sf->chain_sequence[i] = 0;
  }
  sf->pattern_current = 0;
  for (uint8_t i = 0; i < 16; i++) {
    sf->pattern_length[i] = 0;
    for (uint8_t j = 0; j < 128; j++) {
      sf->pattern_sequence[i][j] = 0;
    }
  }
  return sf;
}

bool SaveFile_PatternRandom(SaveFile *sf, pcg32_random_t *rng,
                            uint8_t pattern_id, uint8_t pattern_length) {
  sf->pattern_length[pattern_id] = pattern_length;
  for (uint8_t j = 0; j < sf->pattern_length[pattern_id]; j++) {
    sf->pattern_sequence[pattern_id][j] = (int)pcg32_boundedrand_r(rng, 16) + 0;
  }

  return true;
}

void SaveFile_PatternPrint(SaveFile *sf) {
  for (uint8_t i = 0; i < 16; i++) {
    if (sf->pattern_length[i] > 0) {
      printf("pattern %d\n", i);
      for (uint8_t j = 0; j < sf->pattern_length[i]; j++) {
        printf("%d ", sf->pattern_sequence[i][j]);
      }
      printf("\n");
    }
  }
}

#ifndef NOSDCARD

bool SaveFile_Load(SaveFile *sf) {
  FIL fil; /* File object */
  printf("[SaveFile] reading\n");
  if (f_open(&fil, SAVEFILE_PATHNAME, FA_READ)) {
    printf("[SaveFile] no save file, skipping ");
  } else {
    unsigned int bytes_read;
    if (f_read(&fil, sf, sizeof(SaveFile), &bytes_read)) {
      printf("[SaveFile] problem reading save file");
    } else {
      printf("[SaveFile] bpm_tempo = %d\n", sf->bpm_tempo);
    }
  }
  f_close(&fil);
  return true;
}

bool SaveFile_Save(SaveFile *sf, bool *sync_sd_card) {
  while (*sync_sd_card) {
    sleep_us(100);
  }
  *sync_sd_card = true;
  printf("[SaveFile] writing\n");
  FRESULT fr;
  FIL file; /* File object */
  printf("[SaveFile] opening savefile for writing\n");
  fr = f_open(&file, SAVEFILE_PATHNAME, FA_WRITE | FA_CREATE_ALWAYS);
  if (FR_OK != fr) {
    printf("f_open error: %s (%d)\n", FRESULT_str(fr), fr);
    *sync_sd_card = false;
    return false;
  }
  unsigned int bw;
  if (f_write(&file, sf, sizeof(SaveFile), &bw)) {
    printf("[SaveFile] problem writing save\n");
  }
  printf("[SaveFile] wrote %d bytes\n", bw);
  f_close(&file);
  *sync_sd_card = false;
  return true;
}

#endif
