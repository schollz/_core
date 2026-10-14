#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include "startup_playback_settings.h"

static bool read_failure, close_failure, short_read;
FRESULT f_open(FIL *file, const char *name, int mode) {
  assert(mode == FA_READ);
  file->stream = fopen(name, "rb");
  if (!file->stream) return errno == ENOENT ? FR_NO_FILE : FR_DISK_ERR;
  fseek(file->stream, 0, SEEK_END); file->size = ftell(file->stream);
  rewind(file->stream); return FR_OK;
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
  FILE *file = fopen(filename, "wb"); assert(file);
  assert(fwrite(contents, 1, strlen(contents), file) == strlen(contents));
  assert(fclose(file) == 0);
}
static bool load(void) {
  return startup_playback_load("settings", startup_playback_load("", false));
}
int main(void) {
  assert(!load());
  write_setting("startup_playback", "wait\n"); assert(load());
  assert(mkdir("settings", 0700) == 0);
  for (unsigned i = 0; i < 2; ++i) {
    write_setting("settings/startup_playback", i ? "wait\r\n" : "play\n");
    assert(load() == (i != 0));
  }
  const char *invalid[] = {"", "Wait", " wait", "waitx", "play\nwait", "xxxxxxxxxxxxxxxxx"};
  for (unsigned i = 0; i < sizeof invalid / sizeof *invalid; ++i) {
    write_setting("settings/startup_playback", invalid[i]); assert(!load());
  }
  bool wait = true;
  assert(!startup_playback_parse("wait\0garbage", 12, &wait) && !wait);
  write_setting("settings/startup_playback", "wait\n");
  read_failure = true; assert(!load()); read_failure = false;
  short_read = true; assert(!load()); short_read = false;
  close_failure = true; assert(!load()); close_failure = false;
  assert(load());
  assert(remove("settings/startup_playback") == 0); assert(load());
  puts("Startup playback settings: default, precedence, malformed files and IO failures passed");
}
