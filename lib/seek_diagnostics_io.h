// Copyright 2026 Zack Scholl, GPLv3.0
#ifndef SEEK_DIAGNOSTICS_IO_H
#define SEEK_DIAGNOSTICS_IO_H
#include "seek_diagnostics.h"
#include "ff.h"
#if defined(SEEK_DIAGNOSTICS) && SEEK_DIAGNOSTICS
#include "pico/platform.h"
static inline FRESULT zd_f_open(FIL *file, const char *name, BYTE mode) {
  uint32_t start = time_us_32();
  FRESULT result = f_open(file, name, mode);
  uint32_t elapsed = time_us_32() - start;
  if (get_core_num() == 1) {
    zd_metric_record(&zd_audio.metrics[ZD_OPEN], elapsed, result, 0, false,
                     &zd_audio.counters[ZD_SATURATED]);
    if (!result) zd_audio_counter(ZD_FILE_GENERATION);
  } else {
    zd_metric_record(&zd_control.metrics[ZD_CONTROL_OPEN], elapsed, result,
                     0, false, &zd_control.counters[9]);
    if (!result) ++zd_control.counters[12];
  }
  return result;
}
static inline FRESULT zd_f_close(FIL *file) {
  uint32_t start = time_us_32();
  FRESULT result = f_close(file);
  uint32_t elapsed = time_us_32() - start;
  if (get_core_num() == 1)
    zd_metric_record(&zd_audio.metrics[ZD_CLOSE], elapsed, result, 0, false,
                     &zd_audio.counters[ZD_SATURATED]);
  else
    zd_metric_record(&zd_control.metrics[ZD_CONTROL_CLOSE], elapsed, result,
                     0, false, &zd_control.counters[9]);
  return result;
}
static inline FRESULT zd_f_lseek(FIL *file, FSIZE_t offset, unsigned kind) {
  FSIZE_t from = f_tell(file);
  uint32_t start = time_us_32();
  FRESULT result = f_lseek(file, offset);
  zd_record_seek(kind, start, result, from, offset, f_tell(file),
                  file->cltbl != NULL);
  return result;
}
static inline FRESULT zd_f_read(FIL *file, void *buffer, UINT bytes,
                                UINT *read, unsigned kind) {
  *read = 0;
  uint32_t start = time_us_32();
  FRESULT result = f_read(file, buffer, bytes, read);
  zd_record_io(kind, start, result, *read, *read < bytes);
  return result;
}
#else
#define zd_f_open f_open
#define zd_f_close f_close
#define zd_f_lseek(file, offset, kind) f_lseek(file, offset)
#define zd_f_read(file, buffer, bytes, read, kind) f_read(file, buffer, bytes, read)
#endif
#endif
