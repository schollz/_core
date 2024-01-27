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

#ifndef LIB_SAVEFILE
#define LIB_SAVEFILE 1

#include "sequencer.h"

typedef struct SaveFile {
  uint32_t vol : 9;
  uint32_t bpm_tempo : 9;
  uint32_t bank : 7;
  uint32_t sample : 7;
  Sequencer *sequencers[3][16];
  uint8_t sequence_sel[3];
  bool fx_active[16];
  uint8_t fx_param[16][3];
} SaveFile;

#define SAVEFILE_PATHNAME "save.bin"
void test_sequencer_emit(uint8_t key) { printf("key %d\n", key); }
void test_sequencer_stop() { printf("stop\n"); }
SaveFile *SaveFile_malloc() {
  SaveFile *sf;
  sf = malloc(sizeof(SaveFile) + (sizeof(Sequencer) * 3 * 16));
  sf->bank = 0;
  sf->sample = 0;
  sf->vol = 120;
  sf->bpm_tempo = 165;
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 16; j++) {
      sf->sequencers[i][j] = Sequencer_malloc();
    }
  }
  sf->sequence_sel[0] = 0;
  sf->sequence_sel[1] = 0;
  sf->sequence_sel[2] = 0;

  for (uint8_t i = 0; i < 16; i++) {
    sf->fx_active[i] = false;
    sf->fx_param[i][0] = 0;
    sf->fx_param[i][1] = 0;
    sf->fx_param[i][2] = 0;
  }
  sf->fx_param[FX_SATURATE][0] = 64;
  sf->fx_param[FX_SHAPER][0] = 250;
  sf->fx_param[FX_SHAPER][1] = 177;
  sf->fx_param[FX_SHAPER][2] = 60;
  sf->fx_param[FX_FUZZ][0] = 80;
  sf->fx_param[FX_FUZZ][1] = 40;
  sf->fx_param[FX_BITCRUSH][0] = 218;
  sf->fx_param[FX_BITCRUSH][1] = 90;
  sf->fx_param[FX_DELAY][0] = 200;
  sf->fx_param[FX_DELAY][1] = 200;
  sf->fx_param[FX_BEATREPEAT][0] = 45;
  sf->fx_param[FX_TIGHTEN][0] = 215;
  sf->fx_param[FX_SCRATCH][0] = 132;
  return sf;
}

void SaveFile_test_sequencer(SaveFile *sf) {
  Sequencer_set_callbacks(sf->sequencers[0][sf->sequence_sel[0]],
                          test_sequencer_emit, test_sequencer_stop);
  Sequencer_add(sf->sequencers[0][sf->sequence_sel[0]], 1, 1);
  Sequencer_add(sf->sequencers[0][sf->sequence_sel[0]], 2, 3);
  Sequencer_add(sf->sequencers[0][sf->sequence_sel[0]], 3, 7);
  Sequencer_add(sf->sequencers[0][sf->sequence_sel[0]], 4, 11);
  Sequencer_add(sf->sequencers[0][sf->sequence_sel[0]], 5, 15);
  Sequencer_play(sf->sequencers[0][sf->sequence_sel[0]], false);
  for (int i = 0; i < 18; i++) {
    printf("step %d ", i);
    Sequencer_step(sf->sequencers[0][sf->sequence_sel[0]], i);
    printf("\n");
  }
}

void SaveFile_free(SaveFile *sf) {
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 16; j++) {
      Sequencer_free(sf->sequencers[i][j]);
    }
  }
  free(sf);
}

#ifndef NOSDCARD

bool SaveFile_load(SaveFile *sf, bool *sync_sd_card, uint8_t savefile_index) {
  while (*sync_sd_card) {
    sleep_us(100);
  }
  *sync_sd_card = true;
  FIL fil; /* File object */
  char fname[32];
  sprintf(fname, "savefile%d", savefile_index);
  printf("[SaveFile] reading %s\n", fname);
  if (f_open(&fil, fname, FA_READ)) {
    printf("[SaveFile] no save file, skipping ");
  } else {
    unsigned int bytes_read;
    if (f_read(&fil, sf, sizeof(SaveFile) + (sizeof(Sequencer) * 3 * 16),
               &bytes_read)) {
      printf("[SaveFile] problem reading save file");
    } else {
      printf("[SaveFile] bpm_tempo = %d\n", sf->bpm_tempo);
    }
  }
  f_close(&fil);
  *sync_sd_card = false;
  return true;
}

bool SaveFile_save(SaveFile *sf, bool *sync_sd_card, uint8_t savefile_index) {
  while (*sync_sd_card) {
    sleep_us(100);
  }
  *sync_sd_card = true;
  printf("[SaveFile] writing\n");
  FRESULT fr;
  FIL file; /* File object */
  char fname[32];
  sprintf(fname, "savefile%d", savefile_index);
  printf("[SaveFile] opening savefile for writing\n");
  fr = f_open(&file, fname, FA_WRITE | FA_CREATE_ALWAYS);
  if (FR_OK != fr) {
    printf("f_open error: %s (%d)\n", FRESULT_str(fr), fr);
    *sync_sd_card = false;
    return false;
  }
  unsigned int bw;
  if (f_write(&file, sf, sizeof(SaveFile) + (sizeof(Sequencer) * 3 * 16),
              &bw)) {
    printf("[SaveFile] problem writing save\n");
  }
  printf("[SaveFile] wrote %d bytes\n", bw);
  f_close(&file);
  *sync_sd_card = false;
  return true;
}

#endif

#endif