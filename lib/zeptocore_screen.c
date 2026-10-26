// Copyright 2026 Zack Scholl, GPLv3.0
#include "zeptocore_screen.h"
#include "bank_metadata.h"
#include "ff.h"
#include "screen_waveform_format.h"
#include "sharp_memory.h"
#include "pico/stdlib.h"
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern const uint8_t font_8x5[]; // shared with the existing SSD1306 renderer
#ifdef INCLUDE_MIDI
#include "tusb.h"
#endif

// Fixed total storage stays below 8 KiB, including the driver's DMA packet.
// Reserve once after the existing DSP sizing. Allocating this cache before
// optional reverb would alter its memory-based comb-count selection.
static uint8_t (*peaks)[SW_COLUMNS];
static uint8_t *scratch; // existing 512-byte foreground metadata workspace
static uint8_t boundary_bits[SW_COLUMNS / 8];
static FIL waveform_file;
static struct {
  unsigned bank, selected, cursor, stage;
  bool opened, pending, acceptable;
  uint32_t started;
} load;
static _Atomic uint32_t revision, snapshot_words[8];
static screen_snapshot frame;
_Static_assert(sizeof(screen_snapshot) == 8 * sizeof(uint32_t), "snapshot word layout");
static unsigned boundary_count, row = SHARP_MEMORY_HEIGHT;
static unsigned cached_bank = 16, cached_slot = 16, cached_generation;
static unsigned active_first, active_last, playhead;
static bool enabled, available;
static uint32_t next_frame;
static char title[40], footer[40];
volatile screen_status_t screen_status = {.cache_bank = 16};

void screen_publish(const screen_snapshot *s) {
  uint32_t words[8];
  memcpy(words, s, sizeof words);
  uint32_t serial = atomic_load(&revision);
  atomic_store(&revision, serial + 1);
  for (unsigned n = 0; n < 8; ++n) atomic_store(&snapshot_words[n], words[n]);
  atomic_store(&revision, serial + 2);
}
static bool read_snapshot(screen_snapshot *s) {
  uint32_t before = atomic_load(&revision), words[8];
  if (before & 1) return false;
  for (unsigned n = 0; n < 8; ++n) words[n] = atomic_load(&snapshot_words[n]);
  if (before != atomic_load(&revision)) return false;
  memcpy(s, words, sizeof words);
  return true;
}
void screen_waveforms_cancel(void) {
  if (load.opened) { f_close(&waveform_file); load.opened = false; }
  load.pending = false;
  screen_status.cache_valid = 0;
  screen_status.cache_bank = 16;
}
void screen_waveforms_begin(unsigned bank, unsigned selected_slot, uint32_t now) {
  screen_waveforms_cancel();
  memset(&load, 0, sizeof load);
  load.bank = bank; load.selected = selected_slot; load.started = now;
  load.pending = bank < 16 && peaks != NULL;
  screen_status.cache_bank = bank;
}
bool screen_waveforms_pending(void) { return load.pending; }
bool screen_waveforms_step(uint32_t now, uint32_t deadline) {
  if (!load.pending) return true;
  // Completion takes precedence over the optional deadline. The final close
  // may finish just before the deadline and the next foreground poll after it.
  if (load.cursor >= 16) {
    load.pending = false;
    screen_status.loading_us = now - load.started;
    return true;
  }
  if ((int32_t)(now - deadline) >= 0) {
    if (load.opened) { f_close(&waveform_file); load.opened = false; }
    load.stage = 0;
    return true;
  }
  // Selected slot first, then the other physical slots in ascending order.
  unsigned slot = load.cursor == 0 ? load.selected : load.cursor - 1;
  if (load.cursor && slot >= load.selected) ++slot;
  int ordinal = metadata_ordinal(load.bank, slot);
  if (ordinal < 0) { ++load.cursor; return false; }
  const SampleInfo *s = banks[load.bank]->sample[ordinal].snd[0];
  if (load.stage == 0) {
    char path[40];
    snprintf(path, sizeof path, "bank%u/%u.0.wav.wfm", load.bank + 1, slot);
    FRESULT result = f_open(&waveform_file, path, FA_READ);
    if (result != FR_OK) {
      if (result == FR_NO_FILE || result == FR_NO_PATH) ++screen_status.missing;
      else ++screen_status.io_errors;
      ++load.cursor;
      return false;
    }
    load.opened = true; load.stage = 1; load.acceptable = false;
  } else if (load.stage == 1) {
    UINT got = 0;
    if (f_size(&waveform_file) == SW_FILE_BYTES &&
        f_read(&waveform_file, scratch, SW_FILE_BYTES, &got) == FR_OK &&
        sw_valid(scratch, got, s->size, 44100u * (s->oversampling + 1),
                 s->num_channels + 1, s->metadata_checksum)) {
      memcpy(peaks[slot], scratch + SW_HEADER_BYTES, SW_COLUMNS);
      load.acceptable = true;
    } else ++screen_status.invalid;
    load.stage = 2;
  } else {
    FRESULT result = f_close(&waveform_file);
    load.opened = false;
    if (result == FR_OK && load.acceptable) {
      screen_status.cache_valid |= 1u << slot;
      ++screen_status.loaded;
    } else if (result != FR_OK) ++screen_status.io_errors;
    ++load.cursor; load.stage = 0;
  }
  return false;
}

void screen_init(void) {
  scratch = metadata_foreground_workspace();
  if (!peaks) peaks = malloc(16 * SW_COLUMNS);
  if (!peaks) ++screen_status.cache_no_memory;
  sharp_memory_init();
  sharp_memory_clear();
  enabled = sharp_memory_async_init();
  if (!enabled) ++screen_status.transfers_failed;
}
void screen_boot_startup(unsigned bank, unsigned selected_slot) {
  screen_init();
  uint32_t start = time_us_32();
  screen_waveforms_begin(bank, selected_slot, start);
  while (!screen_waveforms_step(time_us_32(), start + 250000u)) {
#ifdef INCLUDE_MIDI
    if (tud_inited()) tud_task();
#endif
  }
}
static unsigned column(uint32_t position, uint32_t size) {
  if (!size) return 0;
  uint32_t x = (uint32_t)((uint64_t)position * SW_COLUMNS / size);
  return x > SW_COLUMNS ? SW_COLUMNS : x;
}
static void set_black(unsigned x, unsigned y) {
  scratch[(y % 8) * SHARP_MEMORY_ROW_BYTES + x / 8] &= (uint8_t)~(1u << (x % 8));
}
static void draw_text_line(unsigned y, int top, const char *text, unsigned scale) {
  if ((int)y < top || (int)y >= top + 8 * (int)scale) return;
  unsigned dy = (y - top) / scale;
  unsigned x = 8;
  for (; *text && x + 5 * scale < SHARP_MEMORY_WIDTH; ++text, x += 6 * scale) {
    unsigned ch = (unsigned char)*text;
    if (ch < 32 || ch > 126) ch = '?';
    const uint8_t *glyph = font_8x5 + 5 + (ch - 32) * 5;
    for (unsigned dx = 0; dx < 5; ++dx)
      if (glyph[dx] & (1u << dy))
        for (unsigned n = 0; n < scale; ++n) set_black(x + dx * scale + n, y);
  }
}
static void raster_line(unsigned y) {
  draw_text_line(y, 8, title, 2);
  draw_text_line(y, 216, footer, 2);
  if (!available) {
    draw_text_line(y, 112, "WAVEFORM UNAVAILABLE", 2);
    return;
  }
  if (y < 40 || y >= 200) return;
  for (unsigned x = 0; x < SW_COLUMNS; ++x) {
    unsigned height = (peaks[frame.slot][x] * 72u + 127) / 255;
    bool black = height && y >= 120 - height && y < 120 + height;
    if (x >= active_first && x < active_last) black = !black;
    if (x == playhead || x == playhead + 1) black = !black;
    if (black) set_black(8 + x, y);
  }
  if ((y - 40) % 4 == 0)
    for (unsigned x = 0; x < SW_COLUMNS; ++x)
      if (boundary_bits[x / 8] & (1u << (x % 8))) set_black(8 + x, y);
}
void screen_service(uint32_t now) {
  if (!enabled) return;
  sharp_memory_poll();
  if (sharp_memory_busy()) return;
  if (row >= SHARP_MEMORY_HEIGHT) {
    if ((int32_t)(now - next_frame) < 0) return;
    if (!read_snapshot(&frame)) { ++screen_status.snapshot_misses; return; }
    next_frame = now + 200000u;
    available = (frame.flags & ZS_VALID) && frame.bank == screen_status.cache_bank &&
        frame.slot < 16 && (screen_status.cache_valid & (1u << frame.slot)) &&
        frame.generation == metadata_generation() && metadata_ready(frame.bank);
    if (cached_bank != frame.bank || cached_slot != frame.slot ||
        cached_generation != frame.generation ||
        (!boundary_count && (frame.flags & ZS_VALID))) {
      boundary_count = 0;
      memset(boundary_bits, 0, sizeof boundary_bits);
      cached_bank = frame.bank; cached_slot = frame.slot; cached_generation = frame.generation;
      if ((frame.flags & ZS_VALID) && frame.generation == metadata_generation() &&
          metadata_ready(frame.bank)) {
        int ordinal = metadata_ordinal(frame.bank, frame.slot);
        if (ordinal >= 0) {
          const SampleInfo *s = banks[frame.bank]->sample[ordinal].snd[0];
          if (s->slice_start) {
            boundary_count = s->slice_num;
            for (unsigned n = 0; n < s->slice_num; ++n) {
              unsigned x = column(s->slice_start[n], s->size);
              if (x >= SW_COLUMNS) x = SW_COLUMNS - 1;
              boundary_bits[x / 8] |= (uint8_t)(1u << (x % 8));
            }
          }
        }
      }
    }
    active_first = active_last = 0;
    if (available) {
      int ordinal = metadata_ordinal(frame.bank, frame.slot);
      const SampleInfo *s = ordinal < 0 ? NULL : banks[frame.bank]->sample[ordinal].snd[0];
      if (s && s->slice_start && frame.slice < s->slice_num) {
        active_first = column(s->slice_start[frame.slice], s->size);
        active_last = column(s->slice_stop[frame.slice], s->size);
      }
    }
    playhead = column(frame.phase, frame.size);
    if (playhead >= SW_COLUMNS) playhead = SW_COLUMNS - 1;
    snprintf(title, sizeof title, "BANK %02u SAMPLE %02u  %u BPM",
             (unsigned)(frame.bank & 15) + 1, (unsigned)(frame.slot & 15) + 1, (unsigned)(frame.bpm & 511));
    snprintf(footer, sizeof footer, "SLICE %02u/%02u %s %s", (unsigned)(frame.slice & 255) + 1,
             boundary_count & 255, frame.flags & ZS_STOPPED ? "STOP" : frame.flags & ZS_MUTED ? "MUTE" : "PLAY",
             frame.flags & ZS_FORWARD ? ">" : "<");
    row = 0;
  }
  // A transition invalidates the old cache immediately, including mid-frame.
  if (available && (frame.bank != screen_status.cache_bank ||
                   frame.generation != metadata_generation() || !metadata_ready(frame.bank))) {
    row = SHARP_MEMORY_HEIGHT; next_frame = now; return;
  }
  memset(scratch, 0xff, 8 * SHARP_MEMORY_ROW_BYTES);
  for (unsigned y = row; y < row + 8; ++y) raster_line(y);
  if (!sharp_memory_submit_rows(row, 8, scratch)) {
    ++screen_status.transfers_failed; return;
  }
  row += 8;
  if (row == SHARP_MEMORY_HEIGHT) ++screen_status.frames;
}
