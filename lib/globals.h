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

#ifndef GLOBALS_LIB
#define GLOBALS_LIB 1

// audio_pool.h
audio_buffer_pool_t *ap;

FIL fil_current;
char *fil_current_name;
bool fil_is_open;
uint8_t cpu_utilization;
uint8_t fil_buf[SAMPLES_PER_BUFFER * 4];
int32_t phases[2];
int32_t phases_old[2];
int32_t phase_new;
int16_t mem_samples[2][44100];
uint16_t mem_index[2];
bool mem_use;
bool phase_change;
unsigned int fil_bytes_read;
unsigned int fil_bytes_read2;
// uint16_t sf->bpm_tempo = 185;
uint16_t bpm_last = 185;
uint8_t sel_sample_cur = 0;
uint8_t sel_sample_next = 0;
uint8_t sel_bank_cur = 0;
uint8_t sel_bank_next = 0;
uint8_t sel_bank_select = 0;
bool fil_current_change = false;
SampleList *banks[16];
uint8_t banks_with_samples[16];
uint8_t banks_with_samples_num = 0;

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
Noise *noise_wobble;
uint vols[2];

float vol3 = 0;
float envelope_pitch_val;
float envelope_wobble_val;
uint beat_current = 0;
uint beat_total = 0;
uint debounce_quantize = 0;
uint32_t bpm_timer_counter = 0;
uint8_t retrig_beat_num = 0;
uint16_t retrig_timer_reset = 96;
bool retrig_first = false;
bool retrig_ready = false;
float retrig_vol = 1.0;
float retrig_vol_step = 0;

// buttons
// mode toggles
//   mode  ==0  ==1
bool mode_jump_mash = 0;
bool mode_mute = 0;
bool mode_play = 0;

SaveFile *sf;
LEDS *leds;

Chain *chain;
bool toggle_chain_play = false;
bool toggle_chain_rec = false;

int16_t dub_step_break = -1;
uint16_t dub_step_divider = 0;
uint8_t dub_step_beat = 0;

bool gate_active = false;
bool gate_is_applied = false;
uint32_t gate_counter = 0;
uint32_t gate_threshold = 10;

// add variable to keep track of variation
uint8_t sel_variation = 0;
int8_t sel_variation_next = 0;

#ifdef INCLUDE_BASS
Bass *bass;
#endif

ResonantFilter *lowpassFilter[2];
uint8_t filter_midi = 72;

bool sdcard_startup_is_starting = false;
bool audio_mute = false;

#ifdef INCLUDE_RGBLED
WS2812 *ws2812;
#endif

uint8_t key_jump_debounce = 0;
inline void do_update_phase_from_beat_current() {
  // printf("[do_update_phase_from_beat_current] beat_current: %d\n",
  // beat_current);
  uint16_t slice =
      beat_current %
      banks[sel_bank_cur]->sample[sel_sample_cur].snd[sel_variation]->slice_num;
  banks[sel_bank_cur]
      ->sample[sel_sample_cur]
      .snd[sel_variation]
      ->slice_current = slice;
  if (phase_forward) {
    phase_new = banks[sel_bank_cur]
                    ->sample[sel_sample_cur]
                    .snd[sel_variation]
                    ->slice_start[slice];
  } else {
    phase_new = banks[sel_bank_cur]
                    ->sample[sel_sample_cur]
                    .snd[sel_variation]
                    ->slice_stop[slice];
  }
  gate_counter = 0;
  phase_change = true;
  audio_mute = false;
  // printf("do_update_phase_from_beat_current: %d\n", phase_new);
}
#endif
