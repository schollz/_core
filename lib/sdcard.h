// Copyright 2023-2024 Zack Scholl.
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

static sd_card_t *sd_get_by_name(const char *const name) {
  for (size_t i = 0; i < sd_get_num(); ++i)
    if (0 == strcmp(sd_get_by_num(i)->pcName, name)) return sd_get_by_num(i);
  DBG_PRINTF("%s: unknown name %s\n", __func__, name);
  return NULL;
}
static FATFS *sd_get_fs_by_name(const char *name) {
  for (size_t i = 0; i < sd_get_num(); ++i)
    if (0 == strcmp(sd_get_by_num(i)->pcName, name))
      return &sd_get_by_num(i)->fatfs;
  DBG_PRINTF("%s: unknown name %s\n", __func__, name);
  return NULL;
}

void sd_unmount() { f_unmount(sd_get_by_num(0)->pcName); }

bool run_mount() {
  const char *arg1 = strtok(NULL, " ");
  arg1 = sd_get_by_num(0)->pcName;
  FATFS *p_fs = sd_get_fs_by_name(arg1);
  if (!p_fs) {
    // printf("Unknown logical drive number: \"%s\"\n", arg1);
    return false;
  }
  FRESULT fr = f_mount(p_fs, arg1, 1);
  if (FR_OK != fr) {
    // printf("f_mount error: %s (%d)\n", FRESULT_str(fr), fr);
    return false;
  }
  sd_card_t *pSD = sd_get_by_name(arg1);
  myASSERT(pSD);
  pSD->mounted = true;
  return true;
}

// static void run_cat() {
//   char *arg1 = strtok(NULL, " ");
//   if (!arg1) {
//     arg1 = "2.raw";
//   }
//   FIL fil;
//   FRESULT fr = f_open(&fil, arg1, FA_READ);
//   if (FR_OK != fr) {
//     printf("f_open error: %s (%d)\n", FRESULT_str(fr), fr);
//     return;
//   }
//   char buf[256];
//   unsigned int bytes_read;
//   unsigned int pos;
//   f_lseek(&fil, 1);
//   f_read(&fil, buf, 256, &bytes_read);
//   printf("bytes_read: %d\n");
//   pos += bytes_read;
//   printf("run_cat read %d bytes\n", bytes_read);
//   fr = f_close(&fil);
//   if (FR_OK != fr) printf("f_open error: %s (%d)\n", FRESULT_str(fr), fr);
// }

#define FF_MAX_SS 512
#define BUFFSZ (32 * FF_MAX_SS)  // Should be a factor of 1 Mebibyte

#define PRE_ALLOCATE false

typedef uint32_t DWORD;
typedef unsigned int UINT;

// Create a file of size "size" bytes filled with random data seeded with "seed"
static bool create_big_file(const char *const pathname, uint64_t size,
                            unsigned seed, DWORD *buff) {
  FRESULT fr;
  FIL file; /* File object */

  srand(seed);  // Seed pseudo-random number generator

  printf("Writing...\n");
  absolute_time_t xStart = get_absolute_time();

  /* Open the file, creating the file if it does not already exist. */
  FILINFO fno;
  size_t fsz = 0;
  fr = f_stat(pathname, &fno);
  if (FR_OK == fr) fsz = fno.fsize;
  if (0 < fsz && fsz <= size) {
    // This is an attempt at optimization:
    // rewriting the file should be faster than
    // writing it from scratch.
    fr = f_open(&file, pathname, FA_READ | FA_WRITE);
    if (FR_OK != fr) {
      printf("f_open error: %s (%d)\n", FRESULT_str(fr), fr);
      return false;
    }
    fr = f_rewind(&file);
    if (FR_OK != fr) {
      printf("f_rewind error: %s (%d)\n", FRESULT_str(fr), fr);
      return false;
    }
  } else {
    fr = f_open(&file, pathname, FA_WRITE | FA_CREATE_ALWAYS);
    if (FR_OK != fr) {
      printf("f_open error: %s (%d)\n", FRESULT_str(fr), fr);
      return false;
    }
  }
  if (PRE_ALLOCATE) {
    FRESULT fr = f_lseek(&file, size);
    if (FR_OK != fr) {
      printf("f_lseek error: %s (%d)\n", FRESULT_str(fr), fr);
      return false;
    }
    if (f_tell(&file) != size) {
      printf("Disk full?\n");
      return false;
    }
    fr = f_rewind(&file);
    if (FR_OK != fr) {
      printf("f_rewind error: %s (%d)\n", FRESULT_str(fr), fr);
      return false;
    }
  }
  for (uint64_t i = 0; i < size / BUFFSZ; ++i) {
    size_t n;
    for (n = 0; n < BUFFSZ / sizeof(DWORD); n++) buff[n] = rand();
    UINT bw;
    fr = f_write(&file, buff, BUFFSZ, &bw);
    if (bw < BUFFSZ) {
      printf("f_write(%s,,%d,): only wrote %d bytes\n", pathname, BUFFSZ, bw);
      return false;
    }
    if (FR_OK != fr) {
      printf("f_write error: %s (%d)\n", FRESULT_str(fr), fr);
      return false;
    }
  }
  /* Close the file */
  f_close(&file);

  int64_t elapsed_us = absolute_time_diff_us(xStart, get_absolute_time());
  float elapsed = elapsed_us / 1E6;
  printf("Elapsed seconds %.3g\n", elapsed);
  printf("Transfer rate %.3g KiB/s (%.3g kB/s) (%.3g kb/s)\n",
         (double)size / elapsed / 1024, (double)size / elapsed / 1000,
         8.0 * size / elapsed / 1000);
  return true;
}

// Read a file of size "size" bytes filled with random data seeded with "seed"
// and verify the data
static bool check_big_file(char *pathname, uint64_t size, uint32_t seed,
                           DWORD *buff) {
  FRESULT fr;
  FIL file; /* File object */

  srand(seed);  // Seed pseudo-random number generator

  fr = f_open(&file, pathname, FA_READ);
  if (FR_OK != fr) {
    printf("f_open error: %s (%d)\n", FRESULT_str(fr), fr);
    return false;
  }
  printf("Reading...\n");
  absolute_time_t xStart = get_absolute_time();

  for (uint64_t i = 0; i < size / BUFFSZ; ++i) {
    UINT br;
    fr = f_read(&file, buff, BUFFSZ, &br);
    if (br < BUFFSZ) {
      printf("f_read(,%s,%d,):only read %u bytes\n", pathname, BUFFSZ, br);
      return false;
    }
    if (FR_OK != fr) {
      printf("f_read error: %s (%d)\n", FRESULT_str(fr), fr);
      return false;
    }
    /* Check the buffer is filled with the expected data. */
    size_t n;
    for (n = 0; n < BUFFSZ / sizeof(DWORD); n++) {
      unsigned int expected = rand();
      unsigned int val = buff[n];
      if (val != expected) {
        printf("Data mismatch at dword %llu: expected=0x%8x val=0x%8x\n",
               (i * sizeof(buff)) + n, expected, val);
        return false;
      }
    }
  }
  /* Close the file */
  f_close(&file);

  int64_t elapsed_us = absolute_time_diff_us(xStart, get_absolute_time());
  float elapsed = elapsed_us / 1E6;
  printf("Elapsed seconds %.3g\n", elapsed);
  printf("Transfer rate %.3g KiB/s (%.3g kB/s) (%.3g kb/s)\n",
         (double)size / elapsed / 1024, (double)size / elapsed / 1000,
         8.0 * size / elapsed / 1000);
  return true;
}
// Specify size in Mebibytes (1024x1024 bytes)
void big_file_test(char *pathname, size_t size_MiB, uint32_t seed) {
  //  /* Working buffer */
  DWORD *buff = malloc(BUFFSZ);
  assert(buff);
  assert(size_MiB);
  if (4095 < size_MiB) {
    printf("Warning: Maximum file size: 2^32 - 1 bytes on FAT volume\n");
  }
  uint64_t size_B = (uint64_t)size_MiB * 1024 * 1024;

  if (create_big_file(pathname, size_B, seed, buff))
    check_big_file(pathname, size_B, seed, buff);

  free(buff);
}