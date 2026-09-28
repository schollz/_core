#ifndef TEST_METADATA_FF_H
#define TEST_METADATA_FF_H
#include <stdint.h>
#include <stddef.h>
typedef unsigned UINT;
typedef int FRESULT;
enum {FR_OK,FR_DISK_ERR,FR_NO_FILE,FR_NO_PATH};
#define FA_READ 1
typedef struct {unsigned bank,slot,pos;size_t size;int open;} FIL;
typedef struct {size_t fsize;} FILINFO;
#define f_size(f) ((f)->size)
FRESULT f_open(FIL*,const char*,unsigned);
FRESULT f_read(FIL*,void*,UINT,UINT*);
FRESULT f_close(FIL*);
FRESULT f_stat(const char*,FILINFO*);
#endif
