// Copyright 2023 Zack Scholl.
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

#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "hardware/adc.h"
#include "hardware/clocks.h"
#include "hardware/pll.h"
#include "hardware/rtc.h"
#include "hardware/structs/clocks.h"
#include "pico/audio_i2s.h"
#include "pico/stdlib.h"
//
#include "ff.h" /* Obtains integer types */
//
#include "diskio.h" /* Declarations of disk functions */
//
#include "f_util.h"
#include "hw_config.h"
#include "my_debug.h"
#include "sd_card.h"
//
#include "lib/pcg_basic.h"
//
#include "lib/array_resample.h"
#include "lib/audio_pool.h"
#include "lib/biquad.h"
#include "lib/envelope2.h"
#include "lib/file_list.h"
#include "lib/savefile.h"
#include "lib/sdcard.h"
#include "lib/wav.h"

#define SAMPLE_RATE 44100
#define BLOCKS_PER_SECOND SAMPLE_RATE / SAMPLES_PER_BUFFER

static const uint32_t PIN_DCDC_PSM_CTRL = 23;
// audio_pool.h
audio_buffer_pool_t *ap;

FIL fil_current;
char *fil_current_name;
bool fil_is_open;
uint8_t cpu_utilization;
uint8_t fil_buf[SAMPLES_PER_BUFFER * 4];
int32_t phase;
int32_t phase2;
int32_t phase_new;
bool phase_change;
unsigned int fil_bytes_read;
unsigned int fil_bytes_read2;
// uint16_t sf->bpm_tempo = 185;
uint16_t bpm_last = 185;
uint16_t fil_current_id = 0;
uint16_t fil_current_id_next = 0;
bool fil_current_change = false;
FileList *file_list;
FRESULT fil_result;
struct repeating_timer timer;
bool phase_forward = 1;
bool sync_using_sdcard = false;

// voice 1 + 2
// voice 1 is always an envelope UP
// voice 2 is always an envelope DOWN
// voice 1 is only voice that jumps
// voice 2 takes place of old voice and continues
Envelope2 *envelope1;
Envelope2 *envelope2;
Envelope2 *envelope3;
Envelope2 *envelope_pitch;
uint vol1 = 0;
uint vol2 = 0;
float vol3 = 0;
float envelope_pitch_val;
uint beat_current = 0;
uint beat_total = 0;
uint debounce_quantize = 0;
uint32_t bpm_timer_counter = 0;
uint16_t bpm_timer_reset = 96;
uint8_t retrig_beat_num = 0;
uint16_t retrig_timer_reset = 96;

SaveFile *sf;

int random_integer_in_range(int min, int max) {
  // Get a random number in the range [0, RAND_MAX).
  int random_number = rand();

  // Calculate the number of possible random numbers in the specified range.
  int range_size = max - min + 1;

  // Subtract the minimum value from the random number to get a number in the
  // range [min, max).
  random_number = random_number % range_size;

  // Add the minimum value back to get the final random number.
  random_number += min;

  return random_number;
}

// timer
bool repeating_timer_callback(struct repeating_timer *t) {
  if (bpm_last != sf->bpm_tempo) {
    bpm_last = sf->bpm_tempo;
    printf("updaing bpm timer: %d\n", cancel_repeating_timer(&timer));
    add_repeating_timer_us(-(round(30000000 / sf->bpm_tempo / 96)),
                           repeating_timer_callback, NULL, &timer);
  }
  bpm_timer_counter++;
  if (retrig_beat_num > 0) {
    if (bpm_timer_counter % retrig_timer_reset == 0) {
      retrig_beat_num--;
      if (fil_is_open && debounce_quantize == 0) {
        phase_new = (file_list->size[fil_current_id]) *
                    ((beat_current % file_list->beats[fil_current_id]) +
                     (1 - phase_forward)) /
                    file_list->beats[fil_current_id];
        phase_change = true;
        printf("current beat: %d, phase_new: %d, cpu util: %d\n", beat_current,
               phase_new, cpu_utilization);
      }
    }
  } else {
    if (bpm_timer_counter % bpm_timer_reset == 0) {
      // keep to the beat
      if (fil_is_open && debounce_quantize == 0) {
        if (beat_current == 0 && !phase_forward) {
          beat_current = file_list->beats[fil_current_id];
        }
        beat_current += (phase_forward * 2 - 1);
        beat_total++;
        if (sf->pattern_on && sf->pattern_length[sf->pattern_current] > 0) {
          beat_current =
              sf->pattern_sequence[sf->pattern_current]
                                  [beat_total %
                                   sf->pattern_length[sf->pattern_current]];
        }
        phase_new = (file_list->size[fil_current_id]) *
                    ((beat_current % file_list->beats[fil_current_id]) +
                     (1 - phase_forward)) /
                    file_list->beats[fil_current_id];
        phase_change = true;
        printf("current beat: %d, phase_new: %d, cpu util: %d\n", beat_current,
               phase_new, cpu_utilization);
      }
      if (debounce_quantize > 0) {
        debounce_quantize--;
      }
    }
  }
  // printf("Repeat at %lld\n", time_us_64());
  return true;
}

bool sdcard_startup_is_starting = false;

void sdcard_startup() {
  if (sdcard_startup_is_starting) {
    return;
  }
  sdcard_startup_is_starting = true;
  fil_is_open = false;
  while (sync_using_sdcard) {
    sleep_us(100);
  }
  sync_using_sdcard = true;
  while (!run_mount()) {
    sleep_ms(200);
  }
  envelope1 = Envelope2_create(BLOCKS_PER_SECOND, 0.01, 1, 1.5);
  envelope2 = Envelope2_create(BLOCKS_PER_SECOND, 1, 0, 0.01);
  envelope3 = Envelope2_create(BLOCKS_PER_SECOND, 0.01, 1.0, 1.5);
  envelope_pitch = Envelope2_create(BLOCKS_PER_SECOND, 0.5, 1.0, 1.5);
  printf("\nz!!\n");
  file_list = list_files("");
  printf("found %d files\n", file_list->num);
  for (int i = 0; i < file_list->num; i++) {
    printf("%s [%d], %d beats, %d bytes\n", file_list->name[i],
           file_list->bpm[i], file_list->beats[i], file_list->size[i]);
  }
  fil_current_id = 0;
  f_open(&fil_current, file_list->name[fil_current_id], FA_READ);
  fil_is_open = true;
  phase_new = 0;
  phase_change = true;
  sync_using_sdcard = false;
  sdcard_startup_is_starting = false;
}

int main() {
  // // Initialize chosen serial port

  // Set PLL_USB 96MHz
  pll_init(pll_usb, 1, 1536 * MHZ, 4, 4);
  clock_configure(clk_usb, 0, CLOCKS_CLK_USB_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB,
                  96 * MHZ, 48 * MHZ);
  // Change clk_sys to be 96MHz.
  clock_configure(clk_sys, CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLKSRC_CLK_SYS_AUX,
                  CLOCKS_CLK_SYS_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB, 96 * MHZ,
                  96 * MHZ);
  // CLK peri is clocked from clk_sys so need to change clk_peri's freq
  clock_configure(clk_peri, 0, CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLK_SYS,
                  96 * MHZ, 96 * MHZ);
  // Reinit uart now that clk_peri has changed
  stdio_init_all();

  // DCDC PSM control
  // 0: PFM mode (best efficiency)
  // 1: PWM mode (improved ripple)
  gpio_init(PIN_DCDC_PSM_CTRL);
  gpio_set_dir(PIN_DCDC_PSM_CTRL, GPIO_OUT);
  gpio_put(PIN_DCDC_PSM_CTRL, 1);  // PWM mode for less Audio noise

  ap = init_audio();

  // load new save file
  sf = SaveFile_New();

  // Implicitly called by disk_initialize,
  // but called here to set up the GPIOs
  // before enabling the card detect interrupt:
  sd_init_driver();

  // init timers
  // Negative delay so means we will call repeating_timer_callback, and call it
  // again 500ms later regardless of how long the callback took to execute
  // add_repeating_timer_ms(-1000, repeating_timer_callback, NULL, &timer);
  // cancel_repeating_timer(&timer);
  add_repeating_timer_us(-(round(30000000 / sf->bpm_tempo / 96)),
                         repeating_timer_callback, NULL, &timer);

  // Loop forever doing nothing
  printf("-/+ to change volume");

  pcg32_random_t rng;
  pcg32_srandom_r(&rng, time_us_64() ^ (intptr_t)&printf, 54u);

  while (true) {
    int c = getchar_timeout_us(0);
    if (c >= 0) {
      if (c == '-' && sf->vol) sf->vol--;
      if ((c == '=' || c == '+') && sf->vol < 256) sf->vol++;
      if (c == ']') {
        if (sf->bpm_tempo < 300) {
          sf->bpm_tempo += 5;
        }
        printf("\nbpm: %d\n\n", sf->bpm_tempo);
      }
      if (c == 'c') {
        SaveFile_PatternRandom(sf, &rng, 0, 3);
        SaveFile_PatternPrint(sf);
        sf->pattern_on = !sf->pattern_on;
      }
      if (c == '[') {
        if (sf->bpm_tempo > 30) {
          sf->bpm_tempo -= 5;
        }
        printf("\nbpm: %d\n\n", sf->bpm_tempo);
      }
      if (c == 'p') {
        phase_forward = !phase_forward;
      }
      if (c == 'w') {
        phase_new = (file_list->size[fil_current_id]) * 0 / 16;
        phase_new = (phase_new / 4) * 4;
        phase_change = true;
        debounce_quantize = 2;
      }
      if (c == 'e') {
        phase_new = (file_list->size[fil_current_id]) * 1 / 16;
        phase_new = (phase_new / 4) * 4;
        phase_change = true;
        debounce_quantize = 2;
      }
      if (c == 'r') {
        // phase_new = (file_list->size[fil_current_id]) * 2 / 16;
        // phase_new = (phase_new / 4) * 4;
        // phase_change = true;
        // debounce_quantize = 2;
        retrig_beat_num = random_integer_in_range(2, 8);
        retrig_timer_reset =
            96 * random_integer_in_range(1, 3) / random_integer_in_range(1, 12);
        float total_time = (float)(retrig_beat_num * retrig_timer_reset * 60) /
                           (float)(96 * sf->bpm_tempo);
        if (total_time < 0.5) {
          total_time *= 2;
          retrig_beat_num *= 2;
        }
        if (total_time < 0.5) {
          total_time *= 2;
          retrig_timer_reset *= 2;
        }
        printf("retrig_beat_num=%d,retrig_timer_reset=%d,total_time=%2.3f s\n",
               retrig_beat_num, retrig_timer_reset, total_time);
        envelope3 =
            Envelope2_create(BLOCKS_PER_SECOND, 0, 1.0, total_time * 2 / 3);
      }
      if (c == 't') {
        phase_new = (file_list->size[fil_current_id]) * 3 / 16;
        phase_new = (phase_new / 4) * 4;
        phase_change = true;
        debounce_quantize = 2;
      }
      if (c == 'y') {
        phase_new = (file_list->size[fil_current_id]) * 4 / 16;
        phase_new = (phase_new / 4) * 4;
        phase_change = true;
        debounce_quantize = 2;
      }
      if (c == 'u') {
        phase_new = (file_list->size[fil_current_id]) * 5 / 16;
        phase_new = (phase_new / 4) * 4;
        phase_change = true;
        debounce_quantize = 2;
      }
      if (c == 'i') {
        phase_new = (file_list->size[fil_current_id]) * 6 / 16;
        phase_new = (phase_new / 4) * 4;
        phase_change = true;
        debounce_quantize = 2;
      }
      if (c == 'i') {
        phase_new = (file_list->size[fil_current_id]) * 7 / 16;
        phase_new = (phase_new / 4) * 4;
        phase_change = true;
        debounce_quantize = 2;
      }
      if (c == 's') {
        phase_new = (file_list->size[fil_current_id]) * 8 / 16;
        phase_new = (phase_new / 4) * 4;
        phase_change = true;
        debounce_quantize = 2;
      }
      if (c == 'd') {
        phase_new = (file_list->size[fil_current_id]) * 9 / 16;
        phase_new = (phase_new / 4) * 4;
        phase_change = true;
        debounce_quantize = 2;
      }
      if (c == 'f') {
        debounce_quantize = 2;
        beat_current = 10;
        phase_new = (file_list->size[fil_current_id]) * 10 / 16;
        phase_new = (phase_new / 4) * 4;
        phase_change = true;
      }
      if (c == 'g') {
        debounce_quantize = 2;
        beat_current = 11;
        phase_new = (file_list->size[fil_current_id]) * 11 / 16;
        phase_new = (phase_new / 4) * 4;
        phase_change = true;
      }
      if (c == 'h') {
        debounce_quantize = 2;
        beat_current = 12;
        phase_new = (file_list->size[fil_current_id]) * 12 / 16;
        phase_new = (phase_new / 4) * 4;
        phase_change = true;
      }
      if (c == 'j') {
        debounce_quantize = 2;
        beat_current = 13;
        phase_new = (file_list->size[fil_current_id]) * 13 / 16;
        phase_new = (phase_new / 4) * 4;
        phase_change = true;
      }
      if (c == 'k') {
        debounce_quantize = 2;
        beat_current = 14;
        phase_new = (file_list->size[fil_current_id]) * 14 / 16;
        phase_new = (phase_new / 4) * 4;
        phase_change = true;
      }
      if (c == 'l') {
        debounce_quantize = 2;
        beat_current = 15;
        phase_new = (file_list->size[fil_current_id]) * 15 / 16;
        phase_new = (phase_new / 4) * 4;
        phase_change = true;
      }
      if (c == 'b') {
        run_mount();
      }
      if (c == 'n') {
        // big_file_test("testfile", 1, 1);
        SaveFile_Load(sf);
      }
      if (c == 'm') {
        SaveFile_Save(sf, &sync_using_sdcard);
      }
      if (c == '1') {
        fil_current_id_next = 0;
        fil_current_change = true;
      }
      if (c == '2') {
        fil_current_id_next = 1;
        fil_current_change = true;
      }
      if (c == '3') {
        fil_current_id_next = 2;
        fil_current_change = true;
      }
      if (c == 'a') {
        envelope3 = Envelope2_create(BLOCKS_PER_SECOND, 0, 1.0, 1.5);
        debounce_quantize = 2;
      }
      if (c == 'q') {
        envelope_pitch = Envelope2_create(BLOCKS_PER_SECOND, 0.25, 1.0, 1);
        debounce_quantize = 2;
      }
      if (c == 'z') {
        debounce_quantize = 2;
        envelope_pitch = Envelope2_create(BLOCKS_PER_SECOND, 1.0, 0.5, 1);
      }
      if (c == 'x') {
        sdcard_startup();
      }
      printf("sf->vol = %d      \r", sf->vol);
    }
  }
}

// audio callback
void i2s_callback_func() {
  clock_t startTime = time_us_64();
  audio_buffer_t *buffer = take_audio_buffer(ap, false);
  if (buffer == NULL) {
    return;
  }
  int32_t *samples = (int32_t *)buffer->buffer->bytes;

  if (sync_using_sdcard) {
    for (uint16_t i = 0; i < buffer->max_sample_count; i++) {
      int32_t value0 = 0;
      samples[i * 2 + 0] = value0 + (value0 >> 16u);  // L
      samples[i * 2 + 1] = samples[i * 2 + 0];        // R = L
    }
    buffer->sample_count = buffer->max_sample_count;
    give_audio_buffer(ap, buffer);
    printf("[i2s_callback_func] sync_using_sdcard being used\n");
    return;
  }

  sync_using_sdcard = true;

  // read files
  if (fil_is_open) {
    // check if the file is the right one
    if (fil_current_change) {
      if (fil_current_id != fil_current_id_next) {
        phase = phase * file_list->size[fil_current_id_next] /
                file_list->size[fil_current_id];
        f_close(&fil_current);  // close and re-open trick
        f_open(&fil_current, file_list->name[fil_current_id_next], FA_READ);
        f_lseek(&fil_current, WAV_HEADER_SIZE + (phase / 2) * 2);
        fil_current_id = fil_current_id_next;
      }
      fil_current_change = false;
    }

    // flag for new phase
    if (phase_change) {
      phase2 = phase;  // old phase
      phase = (phase_new / 2) * 2;
      phase_change = false;
      // initiate transition envelopes
      // jump point envelope grows
      envelope1 = Envelope2_create(BLOCKS_PER_SECOND, 0, 1.0, 0.05);
      // previous point degrades
      envelope2 = Envelope2_create(BLOCKS_PER_SECOND, 1.0, 0, 0.05);
    }

    vol3 = Envelope2_update(envelope3);

    vol1 = (uint)round(Envelope2_update(envelope1) * sf->vol * vol3);
    vol2 = (uint)round(Envelope2_update(envelope2) * sf->vol * vol3);
    // uncomment to turn off dual playheads
    // vol1 = vol;
    // vol2 = 0;

    envelope_pitch_val = Envelope2_update(envelope_pitch);

    // if (vol1 > 0 && vol2 > 0) {
    //   printf("vol1: %d, vol2: %d\n", vol1, vol2);
    // }

    if (vol1 >= 0) {
      uint32_t values_to_read = buffer->max_sample_count *
                                round(sf->bpm_tempo * envelope_pitch_val) /
                                file_list->bpm[fil_current_id];
      int16_t values[values_to_read];  // max limit

      if (f_lseek(&fil_current, WAV_HEADER_SIZE + (phase / 2) * 2)) {
        printf("problem seeking to phase (%d)\n", phase);
        for (uint16_t i = 0; i < buffer->max_sample_count; i++) {
          int32_t value0 = 0;
          samples[i * 2 + 0] = value0 + (value0 >> 16u);  // L
          samples[i * 2 + 1] = samples[i * 2 + 0];        // R = L
        }
        buffer->sample_count = buffer->max_sample_count;
        give_audio_buffer(ap, buffer);
        sync_using_sdcard = false;
        sdcard_startup();
        return;
      }

      if (f_read(&fil_current, values, values_to_read * 2, &fil_bytes_read)) {
        printf("ERROR READING!\n");
        f_close(&fil_current);  // close and re-open trick
        f_open(&fil_current, file_list->name[fil_current_id], FA_READ);
        f_lseek(&fil_current, WAV_HEADER_SIZE + (phase / 2) * 2);
      }
      if (fil_bytes_read < values_to_read * 2) {
        printf("asked for %d bytes, read %d bytes\n", values_to_read * 2,
               fil_bytes_read);
        if (f_lseek(&fil_current, WAV_HEADER_SIZE)) {
          printf("problem seeking to 0\n");
        }
        int16_t values2[values_to_read * 2 - fil_bytes_read];  // max limit
        if (f_read(&fil_current, values2, values_to_read * 2 - fil_bytes_read,
                   &fil_bytes_read2)) {
          printf("ERROR READING!\n");
          f_close(&fil_current);  // close and re-open trick
          f_open(&fil_current, file_list->name[fil_current_id], FA_READ);
          f_lseek(&fil_current, WAV_HEADER_SIZE + (phase / 2) * 2);
        }
        printf("asked for %d bytes, read %d bytes\n",
               values_to_read * 2 - fil_bytes_read, fil_bytes_read2);

        for (uint16_t i = 0; i < fil_bytes_read2 / 2; i++) {
          values[i + fil_bytes_read / 2] = values2[i];
        }
      }

      if (!phase_forward) {
        // reverse audio
        for (int i = 0; i < values_to_read / 2; i++) {
          int16_t temp = values[i];
          values[i] = values[values_to_read - i - 1];
          values[values_to_read - i - 1] = temp;
        }
      }

      int16_t arr_new_size = buffer->max_sample_count;
      int16_t *newArray =
          array_resample_linear(values, values_to_read, arr_new_size);
      for (uint16_t i = 0; i < arr_new_size; i++) {
        int32_t value0 = (vol1 * newArray[i]) << 8u;
        samples[i * 2 + 0] = value0 + (value0 >> 16u);  // L
        samples[i * 2 + 1] = samples[i * 2 + 0];        // R = L
      }
      phase += values_to_read * 2 * (phase_forward * 2 - 1);
      free(newArray);
    }

    if (vol2 > 0) {
      // need to be reading through the second phase!
      uint32_t values_to_read = buffer->max_sample_count *
                                (sf->bpm_tempo * envelope_pitch_val) /
                                file_list->bpm[fil_current_id];
      int16_t values[values_to_read];  // max limit

      if (f_lseek(&fil_current, WAV_HEADER_SIZE + (phase2 / 2) * 2)) {
        printf("problem seeking to phase (%d)\n", phase2);
      }

      if (f_read(&fil_current, values, values_to_read * 2, &fil_bytes_read)) {
        printf("ERROR READING!\n");
        f_close(&fil_current);  // close and re-open trick
        f_open(&fil_current, file_list->name[fil_current_id], FA_READ);
        f_lseek(&fil_current, WAV_HEADER_SIZE + (phase2 / 2) * 2);
      }
      if (fil_bytes_read < values_to_read * 2) {
        printf("asked for %d bytes, read %d bytes\n", values_to_read * 2,
               fil_bytes_read);
        if (f_lseek(&fil_current, WAV_HEADER_SIZE)) {
          printf("problem seeking to 0\n");
        }
        int16_t values2[values_to_read * 2 - fil_bytes_read];  // max limit
        if (f_read(&fil_current, values2, values_to_read * 2 - fil_bytes_read,
                   &fil_bytes_read2)) {
          printf("ERROR READING!\n");
          f_close(&fil_current);  // close and re-open trick
          f_open(&fil_current, file_list->name[fil_current_id], FA_READ);
          f_lseek(&fil_current, WAV_HEADER_SIZE + (phase2 / 2) * 2);
        }
        printf("asked for %d bytes, read %d bytes\n",
               values_to_read * 2 - fil_bytes_read, fil_bytes_read2);

        for (uint16_t i = 0; i < fil_bytes_read2 / 2; i++) {
          values[i + fil_bytes_read / 2] = values2[i];
        }
      }

      if (!phase_forward) {
        // reverse audio
        for (int i = 0; i < values_to_read / 2; i++) {
          int16_t temp = values[i];
          values[i] = values[values_to_read - i - 1];
          values[values_to_read - i - 1] = temp;
        }
      }

      int16_t arr_new_size = buffer->max_sample_count;
      int16_t *newArray =
          array_resample_linear(values, values_to_read, arr_new_size);
      for (uint16_t i = 0; i < arr_new_size; i++) {
        int32_t value0 = (vol2 * newArray[i]) << 8u;
        samples[i * 2 + 0] =
            samples[i * 2 + 0] + value0 + (value0 >> 16u);  // L
        samples[i * 2 + 1] = samples[i * 2 + 0];            // R = L
      }
      phase2 += values_to_read * 2 * (phase_forward * 2 - 1);
      free(newArray);
    }
  }

  // // LPF
  // for (uint16_t i = 0; i < buffer->max_sample_count; i++) {
  //   samples[i * 2 + 0] = filter_lpf(samples[i * 2], 35, 1);
  //   samples[i * 2 + 1] = samples[i * 2];
  // }

  buffer->sample_count = buffer->max_sample_count;
  give_audio_buffer(ap, buffer);

  if (fil_is_open) {
    if (phase >= file_list->size[fil_current_id]) {
      phase -= file_list->size[fil_current_id];
    } else if (phase < 0) {
      phase += file_list->size[fil_current_id];
    }
    if (phase2 >= file_list->size[fil_current_id]) {
      phase2 -= file_list->size[fil_current_id];
    } else if (phase2 < 0) {
      phase2 += file_list->size[fil_current_id];
    }
  }
  sync_using_sdcard = false;

  clock_t endTime = time_us_64();
  cpu_utilization = 100 * (endTime - startTime) / (US_PER_BLOCK);
  return;
}