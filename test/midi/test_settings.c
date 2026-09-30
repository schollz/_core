#include <assert.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

#include "midi_channel_settings.h"

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
static uint8_t load(void) {
  uint8_t channel = midi_channel_load("", 1);
  return midi_channel_load("settings", channel);
}
int main(void) {
  assert(load() == 1);
  write_setting("midi_channel", "10\n"); assert(load() == 10);
  assert(mkdir("settings", 0700) == 0);
  for (unsigned channel = 1; channel <= 16; ++channel) {
    char value[16]; snprintf(value, sizeof value, "%u\r\n", channel);
    write_setting("settings/midi_channel", value); assert(load() == channel);
  }
  read_failure = true; assert(load() == 1); read_failure = false;
  close_failure = true; assert(load() == 1); close_failure = false;
  short_read = true; assert(load() == 1); short_read = false;
  const char *invalid[] = {"", "0", "17", "-1", "1.0", "01", " 2", "10x", "100000000000000000000"};
  for (unsigned i = 0; i < sizeof invalid / sizeof *invalid; ++i) {
    write_setting("settings/midi_channel", invalid[i]); assert(load() == 1);
  }
  assert(remove("settings/midi_channel") == 0); assert(load() == 10);
  puts("MIDI settings: channels, precedence and read failures passed");
}
