// Copyright 2023-2025 Zack Scholl, GPLv3.0

#ifndef LIB_SAVEFILE
#define LIB_SAVEFILE 1

#include "sequencer.h"
#include "utils.h"

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
  uint64_t do_retrig_volume_ramps : 1;
  uint64_t feature_magic : 8;
  uint64_t _padding : 46;
#ifdef INCLUDE_ECTOCORE
  uint16_t center_calibration[8];
#endif
} SaveFile;

#define SAVEFILE_PATHNAME "save.bin"
#define SAVEFILE_FEATURE_MAGIC 0xA5
void SaveFile_sanitize(SaveFile *sf) {
  if (sf->feature_magic != SAVEFILE_FEATURE_MAGIC) {
    sf->do_retrig_volume_ramps = 1;
    sf->feature_magic = SAVEFILE_FEATURE_MAGIC;
  }
}
SaveFile *SaveFile_malloc() {
  SaveFile *sf;
  // Each Sequencer owns a separate allocation below. The previous parent
  // allocation also reserved space for 48 unused Sequencers.
  sf = malloc(sizeof(SaveFile));
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
  sf->do_retrig_volume_ramps = 1;
  sf->feature_magic = SAVEFILE_FEATURE_MAGIC;
  sf->_padding = 0;
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
#ifdef INCLUDE_ECTOCORE
  for (int i = 0; i < 8; i++) {
    sf->center_calibration[i] = 1024 / 2;
  }
#endif
  return sf;
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
  char fname[32];
  format_prefixed_int32(fname, "savefile", savefile_index);
  // load from the file on the file system
  FILE *file = fopen(fname, "rb");
  if (file == NULL) {
    return false;
  }
  fread(sf, sizeof(SaveFile), 1, file);
  SaveFile_sanitize(sf);

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
  format_prefixed_int32(fname, "savefile", savefile_index);
  if (f_open(&fil, fname, FA_READ) == FR_OK) {
    unsigned int bytes_read;
    if (f_read(&fil, sf, sizeof(SaveFile), &bytes_read) == FR_OK) {
      SaveFile_sanitize(sf);
    }
    // read sequencers
    for (int i = 0; i < 3; i++) {
      for (int j = 0; j < 16; j++) {
        f_read(&fil, sf->sequencers[i][j], sizeof(Sequencer), &bytes_read);
      }
    }
  }
  f_close(&fil);
  return true;
}

bool SaveFile_save(SaveFile *sf, uint8_t savefile_index) {
  FRESULT fr;
  FIL file; /* File object */
  char fname[32];

  format_prefixed_int32(fname, "savefile", savefile_index);
  fr = f_open(&file, fname, FA_WRITE | FA_CREATE_ALWAYS);
  if (FR_OK != fr) {
    return false;
  }
  unsigned int bw;
  SaveFile_sanitize(sf);
  f_write(&file, sf, sizeof(SaveFile), &bw);

  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 16; j++) {
      f_write(&file, sf->sequencers[i][j], sizeof(Sequencer), &bw);
      }
    }
  f_close(&file);
  return true;
}

#endif

#endif
