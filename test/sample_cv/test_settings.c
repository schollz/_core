#include <assert.h>
#include <errno.h>
#include <sys/stat.h>

#include "sample_cv_settings.h"

static bool read_failure, close_failure, short_read;
FRESULT f_open(FIL *file, const char *name, int mode) {
  assert(mode == FA_READ);
  file->stream = fopen(name, "rb");
  if (!file->stream) return errno == ENOENT ? FR_NO_FILE : FR_DISK_ERR;
  fseek(file->stream, 0, SEEK_END);
  file->size = ftell(file->stream);
  rewind(file->stream);
  return FR_OK;
}
FRESULT f_read(FIL *file, void *data, UINT count, UINT *read) {
  *read = fread(data, 1, short_read ? 1 : count, file->stream);
  return read_failure ? FR_DISK_ERR : FR_OK;
}
FRESULT f_close(FIL *file) {
  assert(fclose(file->stream) == 0);
  return close_failure ? FR_DISK_ERR : FR_OK;
}
static void write_setting(const char *filename, const char *contents) {
  FILE *file = fopen(filename, "wb");
  assert(file);
  assert(fwrite(contents, 1, strlen(contents), file) == strlen(contents));
  assert(fclose(file) == 0);
}
static SampleCVMapping load(void) {
  SampleCVMapping mapping = sample_cv_load_mapping("", SAMPLE_CV_MAPPING_BANK);
  return sample_cv_load_mapping("settings", mapping);
}
int main(void) {
  assert(load() == SAMPLE_CV_MAPPING_BANK);
  write_setting("sample_cv_mapping", "1voct\n");
  assert(load() == SAMPLE_CV_MAPPING_1VOCT);
  assert(mkdir("settings", 0700) == 0);
  assert(load() == SAMPLE_CV_MAPPING_1VOCT);
  write_setting("settings/sample_cv_mapping", "bank\n");
  assert(load() == SAMPLE_CV_MAPPING_BANK);
  write_setting("settings/sample_cv_mapping", "1voct\r\n");
  assert(load() == SAMPLE_CV_MAPPING_1VOCT);
  read_failure = true;
  assert(load() == SAMPLE_CV_MAPPING_BANK);
  read_failure = false;
  close_failure = true;
  assert(load() == SAMPLE_CV_MAPPING_BANK);
  close_failure = false;
  short_read = true;
  assert(load() == SAMPLE_CV_MAPPING_BANK);
  short_read = false;
  const char *invalid[] = {"", "invalid", "1voctXXX",
                           "1voct01234567890123456789"};
  for (unsigned i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
    write_setting("settings/sample_cv_mapping", invalid[i]);
    assert(load() == SAMPLE_CV_MAPPING_BANK);
  }
  puts("Sample CV settings defaults, overrides, and read failures passed");
}
