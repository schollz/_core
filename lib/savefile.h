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
  sf->distortion_level = 0;
  sf->distortion_wet = 0;
  sf->saturate_wet = 0;
  sf->wavefold = 0;

  return sf;
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
