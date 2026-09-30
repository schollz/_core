#ifndef CORE_MIDI_CHANNEL_SETTINGS_H
#define CORE_MIDI_CHANNEL_SETTINGS_H
#include <stdbool.h>
#include <stdio.h>
#include "ff.h"
#include "midi_channel.h"

static inline uint8_t midi_channel_load(const char *directory, uint8_t current) {
  char filename[64];
  snprintf(filename, sizeof filename, "%s%smidi_channel", directory,
           directory[0] ? "/" : "");
  FIL file;
  FRESULT result = f_open(&file, filename, FA_READ);
  if (result == FR_NO_FILE || result == FR_NO_PATH) return current;
  if (result != FR_OK) return 1;
  char value[16];
  UINT length = 0;
  bool valid = f_size(&file) <= sizeof value;
  if (valid) {
    result = f_read(&file, value, sizeof value, &length);
    valid = result == FR_OK && length == f_size(&file);
  }
  if (f_close(&file) != FR_OK) valid = false;
  uint8_t channel = valid ? midi_channel_parse(value, length) : 0;
  return channel ? channel : 1;
}
#endif
