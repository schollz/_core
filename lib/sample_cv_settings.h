#ifndef SAMPLE_CV_SETTINGS_H
#define SAMPLE_CV_SETTINGS_H

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "ff.h"
#include "sample_cv.h"

// A single file can be overwritten when copying a new settings download.
// Missing files preserve the caller's default or root-directory setting.
static inline SampleCVMapping sample_cv_load_mapping(const char *directory,
                                                     SampleCVMapping current) {
  char filename[64];
  snprintf(filename, sizeof(filename), "%s%ssample_cv_mapping", directory,
           directory[0] ? "/" : "");
  FIL file;
  FRESULT result = f_open(&file, filename, FA_READ);
  if (result == FR_NO_FILE || result == FR_NO_PATH) return current;
  if (result != FR_OK) return SAMPLE_CV_MAPPING_BANK;

  char value[16];
  UINT length = 0;
  bool valid = f_size(&file) <= sizeof(value);
  if (valid) {
    result = f_read(&file, value, sizeof(value), &length);
    valid = result == FR_OK && length == f_size(&file);
  }
  if (f_close(&file) != FR_OK) valid = false;
  if (!valid) return SAMPLE_CV_MAPPING_BANK;
  while (length && isspace((unsigned char)value[length - 1])) --length;
  return length == 5 && memcmp(value, "1voct", 5) == 0 ? SAMPLE_CV_MAPPING_1VOCT
                                                       : SAMPLE_CV_MAPPING_BANK;
}

#endif
