#include <assert.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

#include "start_tempo_settings.h"

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
static uint16_t load(void) {
  return start_tempo_load("settings", start_tempo_load("", 0));
}
int main(void) {
  assert(load() == 0);
  write_setting("start_tempo", "130\n"); assert(load() == 130);
  assert(mkdir("settings", 0700) == 0);
  for (unsigned bpm = 30; bpm <= 300; ++bpm) {
    char value[16]; snprintf(value, sizeof value, "%u\r\n", bpm);
    write_setting("settings/start_tempo", value); assert(load() == bpm);
  }
  read_failure = true; assert(load() == 0); read_failure = false;
  close_failure = true; assert(load() == 0); close_failure = false;
  short_read = true; assert(load() == 0); short_read = false;
  const char *invalid[] = {"", "0", "29", "301", "-1", "130.0", "0130", " 130", "130x", "100000000000000000000"};
  for (unsigned i = 0; i < sizeof invalid / sizeof *invalid; ++i) {
    write_setting("settings/start_tempo", invalid[i]); assert(load() == 0);
  }
  write_setting("settings/start_tempo", "default\r\n"); assert(load() == 0);
  assert(remove("settings/start_tempo") == 0); assert(load() == 130);
  puts("Start tempo: range, default, precedence and read failures passed");
}
