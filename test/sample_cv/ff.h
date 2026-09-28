#ifndef TEST_SAMPLE_CV_FF_H
#define TEST_SAMPLE_CV_FF_H
#include <stdio.h>
typedef unsigned UINT;
typedef enum { FR_OK, FR_DISK_ERR, FR_NO_FILE, FR_NO_PATH } FRESULT;
typedef struct {
  FILE *stream;
  unsigned size;
} FIL;
#define FA_READ 1
#define f_size(file) ((file)->size)
FRESULT f_open(FIL *file, const char *name, int mode);
FRESULT f_read(FIL *file, void *data, UINT count, UINT *read);
FRESULT f_close(FIL *file);
#endif
