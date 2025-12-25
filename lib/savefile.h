// Copyright 2023-2025 Zack Scholl, GPLv3.0

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
  uint64_t stay_in_sync : 1;
  uint64_t pitch_val_index : 7;
  uint64_t do_retrig_pitch_changes : 1;
  uint64_t _padding : 55;
#ifdef INCLUDE_EZEPTOCORE
  uint16_t center_calibration[8];
#endif
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
  sf->bpm_tempo = 170;
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
  sf->do_retrig_pitch_changes = 1;
  sf->stay_in_sync = 1;  // default to staying in sync
  sf->fx_param[FX_SATURATE][0] = 64;
  sf->fx_param[FX_SHAPER][0] = 180;
  sf->fx_param[FX_SHAPER][1] = 28;
  sf->fx_param[FX_FUZZ][0] = 80;
  sf->fx_param[FX_FUZZ][1] = 48;
  sf->fx_param[FX_BITCRUSH][0] = 218;
  sf->fx_param[FX_BITCRUSH][1] = 90;
  sf->fx_param[FX_DELAY][0] = 200;
  sf->fx_param[FX_DELAY][1] = 200;
  sf->fx_param[FX_BEATREPEAT][0] = 45;
  sf->fx_param[FX_TIGHTEN][0] = 215;
  sf->fx_param[FX_SCRATCH][0] = 132;
  sf->fx_param[FX_FILTER][0] = 0;
  sf->fx_param[FX_FILTER][1] = 50;
  sf->fx_param[FX_PAN][0] = 128;
  sf->fx_param[FX_PAN][1] = 255;
  // sf->fx_param[FX_TREMELO][0] = 128;
  // sf->fx_param[FX_TREMELO][1] = 255;
  sf->fx_param[FX_EXPAND][0] = 240;
  sf->fx_param[FX_EXPAND][1] = 120;
  sf->fx_param[FX_REPITCH][0] = 0;
  sf->fx_param[FX_REPITCH][1] = 100;
  sf->fx_param[FX_COMB][0] = 10;
  sf->fx_param[FX_COMB][1] = 10;
  sf->fx_param[FX_TAPE_STOP][0] = 70;
  sf->fx_param[FX_TAPE_STOP][1] = 45;
  sf->pitch_val_index = 48;
#ifdef INCLUDE_EZEPTOCORE
  for (int i = 0; i < 8; i++) {
    sf->center_calibration[i] = 1024 / 2;
  }
#endif
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

#ifdef NOSDCARD
bool SaveFile_load(SaveFile *sf, uint8_t savefile_index) {
  printf("[SaveFile] loading\n");
  char fname[32];
  sprintf(fname, "savefile%d", savefile_index);
  printf("[SaveFile] reading %s\n", fname);
  // load from the file on the file system
  FILE *file = fopen(fname, "rb");
  if (file == NULL) {
    printf("[SaveFile] no save file, skipping ");
    return false;
  }
  fread(sf, sizeof(SaveFile), 1, file);
  // print everything in the savefile
  printf("[SaveFile] vol: %d\n", sf->vol);
  printf("[SaveFile] bpm_tempo: %d\n", sf->bpm_tempo);
  printf("[SaveFile] bank: %d\n", sf->bank);
  printf("[SaveFile] sample: %d\n", sf->sample);
  // print which effects are on
  for (int i = 0; i < 16; i++) {
    printf("[SaveFile] fx_active[%d]: %d\n", i, sf->fx_active[i]);
  }
  // print stay in sync
  printf("[SaveFile] stay_in_sync: %d\n", sf->stay_in_sync);
  // print pitch_val_index
  printf("[SaveFile] pitch_val_index: %d\n", sf->pitch_val_index);
  // print do_retrig_pitch_changes
  printf("[SaveFile] do_retrig_pitch_changes: %d\n",
         sf->do_retrig_pitch_changes);

  // read sequencers
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 16; j++) {
      fread(sf->sequencers[i][j], sizeof(Sequencer), 1, file);
    }
  }
  fclose(file);
  return true;
}
#endif
#ifndef NOSDCARD

bool SaveFile_load(SaveFile *sf, uint8_t savefile_index) {
  FIL fil; /* File object */
  char fname[32];
  sprintf(fname, "savefile%d", savefile_index);
  printf("[SaveFile] reading %s\n", fname);
  if (f_open(&fil, fname, FA_READ)) {
    printf("[SaveFile] no save file, skipping ");
  } else {
    unsigned int bytes_read;
    if (f_read(&fil, sf, sizeof(SaveFile), &bytes_read)) {
      printf("[SaveFile] problem reading save file");
    } else {
      printf("[SaveFile] bpm_tempo = %d\n", sf->bpm_tempo);
    }
    // read sequencers
    for (int i = 0; i < 3; i++) {
      for (int j = 0; j < 16; j++) {
        if (f_read(&fil, sf->sequencers[i][j], sizeof(Sequencer),
                   &bytes_read)) {
          printf("[SaveFile] problem reading sequencer %d %d\n", i, j);
        }
      }
    }
  }
  f_close(&fil);
  return true;
}

bool SaveFile_save(SaveFile *sf, uint8_t savefile_index) {
  printf("[SaveFile] writing\n");
  FRESULT fr;
  FIL file; /* File object */
  char fname[32];

  sprintf(fname, "savefile%d", savefile_index);
  printf("[SaveFile] opening savefile for writing\n");
  fr = f_open(&file, fname, FA_WRITE | FA_CREATE_ALWAYS);
  if (FR_OK != fr) {
    printf("f_open error: %s (%d)\n", FRESULT_str(fr), fr);
    return false;
  }
  unsigned int total_bytes_written;
  unsigned int bw;
  if (f_write(&file, sf, sizeof(SaveFile), &bw)) {
    printf("[SaveFile] problem writing save\n");
  }
  total_bytes_written = bw;

  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 16; j++) {
      if (f_write(&file, sf->sequencers[i][j], sizeof(Sequencer), &bw)) {
        printf("[SaveFile] problem writing sequencer %d %d\n", i, j);
      } else {
        total_bytes_written += bw;
      }
    }
  }
  printf("[SaveFile] wrote %d bytes\n", total_bytes_written);
  f_close(&file);
  return true;
}

#endif

#endif