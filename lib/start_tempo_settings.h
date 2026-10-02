#ifndef CORE_START_TEMPO_SETTINGS_H
#define CORE_START_TEMPO_SETTINGS_H
#include <stdio.h>
#include "ff.h"
#include "start_tempo.h"

static inline uint16_t start_tempo_load(const char *directory, uint16_t current) {
  char filename[64];
  snprintf(filename, sizeof filename, "%s%sstart_tempo", directory,
           directory[0] ? "/" : "");
  FIL file;
  FRESULT result = f_open(&file, filename, FA_READ);
  if (result == FR_NO_FILE || result == FR_NO_PATH) return current;
  if (result != FR_OK) return 0;
  char value[16];
  UINT length = 0;
  bool valid = f_size(&file) <= sizeof value;
  if (valid) {
    result = f_read(&file, value, sizeof value, &length);
    valid = result == FR_OK && length == f_size(&file);
  }
  if (f_close(&file) != FR_OK) valid = false;
  uint16_t bpm = 0;
  if (!valid || !start_tempo_parse(value, length, &bpm)) return 0;
  return bpm;
}
#endif
