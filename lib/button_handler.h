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

#include "globals.h"

// keys
#define KEY_SHIFT 0
#define KEY_A 1
#define KEY_B 2
#define KEY_C 3
#define MODE_JUMP 0
#define MODE_MASH 1
#define MODE_BASS 2

uint8_t key_held_num = 0;
bool key_held_on = false;
int32_t key_timer = 0;
uint8_t key_pressed[100];
uint8_t key_pressed_num = 0;
uint8_t key_total_pressed = 0;
int16_t key_on_buttons[BUTTONMATRIX_BUTTONS_MAX];
int16_t key_on_buttons_last[BUTTONMATRIX_BUTTONS_MAX];
bool key_did_go_off[BUTTONMATRIX_BUTTONS_MAX];
uint16_t key_num_presses;
bool key_b_sample_select = false;

bool button_is_pressed(uint8_t key) { return key_on_buttons[key] > 0; }

void key_do_jump(uint8_t beat) {
  if (beat >= 0 && beat < 16) {
    printf("key_do_jump %d\n", beat);
    key_jump_debounce = 1;
    beat_current = (beat_current / 16) * 16 + beat;
    retrig_pitch = PITCH_VAL_MID;
    do_update_phase_from_beat_current();
  }
}

int8_t single_step_pressed() {
  uint8_t pressed = 0;
  int8_t val = -1;
  for (uint8_t i = 4; i < BUTTONMATRIX_BUTTONS_MAX; i++) {
    if (key_on_buttons[i]) {
      pressed++;
      val = i - 4;
    }
  }
  if (pressed == 1) {
    return val;
  }
  return -1;
}

void go_retrigger_3key(uint8_t key1, uint8_t key2, uint8_t key3) {
  debounce_quantize = 0;
  retrig_first = true;
  retrig_beat_num = key2 + 4;
  retrig_timer_reset = 96 / key3;
  float total_time = (float)(retrig_beat_num * retrig_timer_reset * 60) /
                     (float)(96 * sf->bpm_tempo);
  retrig_vol_step = 1.0 / ((float)retrig_beat_num);
  printf("retrig_beat_num=%d,retrig_timer_reset=%d,total_time=%2.3fs\n",
         retrig_beat_num, retrig_timer_reset, total_time);
  retrig_ready = true;
}

void go_retrigger_2key(uint8_t key1, uint8_t key2) {
  debounce_quantize = 0;
  retrig_first = true;
  retrig_beat_num = random_integer_in_range(8, 24);
  retrig_timer_reset =
      96 * random_integer_in_range(1, 6) / random_integer_in_range(2, 12);
  float total_time = (float)(retrig_beat_num * retrig_timer_reset * 60) /
                     (float)(96 * sf->bpm_tempo);
  if (total_time > 5.0f) {
    total_time = total_time / 2;
    retrig_timer_reset = retrig_timer_reset / 2;
  }
  if (total_time > 5.0f) {
    total_time = total_time / 2;
    retrig_beat_num = retrig_beat_num / 2;
    if (retrig_beat_num == 0) {
      retrig_beat_num = 1;
    }
  }
  if (total_time < 0.5f) {
    total_time = total_time * 2;
    retrig_beat_num = retrig_beat_num * 2;
    if (retrig_beat_num == 0) {
      retrig_beat_num = 1;
    }
  }
  if (total_time < 0.5f) {
    total_time = total_time * 2;
    retrig_beat_num = retrig_beat_num * 2;
    if (retrig_beat_num == 0) {
      retrig_beat_num = 1;
    }
  }
  retrig_vol_step = 1.0 / ((float)retrig_beat_num);
  printf("retrig_beat_num=%d,retrig_timer_reset=%d,total_time=%2.3fs\n",
         retrig_beat_num, retrig_timer_reset, total_time);
  retrig_ready = true;
}

void go_update_top() {
  bool all_off = true;
  for (uint8_t i = 0; i < 4; i++) {
    if (key_on_buttons[i] > 0) {
      all_off = false;
    }
  }

  if (all_off) {
    // top row is not held
    // show jump/mash, mute, play
    // LEDS_set(leds, LED_BASE_FACE, KEY_A, 2 * (1 - mode_buttons16));
    // LEDS_set(leds, LED_BASE_FACE, KEY_B, 2 * mode_mute);
    // LEDS_set(leds, LED_BASE_FACE, KEY_C, 2 * mode_play);
  }
}

// toggle the fx
void toggle_fx(uint8_t fx_num) {
  sf->fx_active[fx_num] = !sf->fx_active[fx_num];
  switch (fx_num) {
    case FX_REVERSE:
      phase_forward = !sf->fx_active[fx_num];
      break;
    case FX_SATURATE:
      Saturation_setActive(saturation, sf->fx_active[fx_num]);
      break;
    case FX_BEATREPEAT:
      if (sf->fx_active[fx_num]) {
        BeatRepeat_repeat(beatrepeat,
                          sf->fx_param[FX_BEATREPEAT][0] * 19000 / 255 + 100);
      } else {
        BeatRepeat_repeat(beatrepeat, 0);
      }
      break;
    case FX_DELAY:
      Delay_setActive(delay, sf->fx_active[fx_num]);
      break;
    case FX_TIGHTEN:
      printf("FX_TIGHTEN\n");
      if (sf->fx_active[fx_num]) {
        Gate_set_percent(audio_gate, 85);
      } else {
        Gate_set_percent(audio_gate, 100);
      }
      break;
    case FX_SLOWDOWN:
      if (sf->fx_active[fx_num]) {
        Envelope2_reset(envelope_pitch, BLOCKS_PER_SECOND,
                        Envelope2_update(envelope_pitch), 0.5, 1);
      } else {
        Envelope2_reset(envelope_pitch, BLOCKS_PER_SECOND,
                        Envelope2_update(envelope_pitch), 1.0, 1);
      }
      break;
    case FX_SPEEDUP:
      if (sf->fx_active[fx_num]) {
        Envelope2_reset(envelope_pitch, BLOCKS_PER_SECOND,
                        Envelope2_update(envelope_pitch), 2.0, 1);
      } else {
        Envelope2_reset(envelope_pitch, BLOCKS_PER_SECOND,
                        Envelope2_update(envelope_pitch), 1.0, 1);
      }
      break;
    case FX_TAPE_STOP:
      if (sf->fx_active[FX_TAPE_STOP]) {
        Envelope2_reset(envelope_pitch, BLOCKS_PER_SECOND,
                        Envelope2_update(envelope_pitch),
                        ENVELOPE_PITCH_THRESHOLD / 2, 2.7);
      } else {
        Envelope2_reset(envelope_pitch, BLOCKS_PER_SECOND,
                        Envelope2_update(envelope_pitch), 1.0, 1.9);
      }
      break;
    case FX_FUZZ:
      if (sf->fx_active[FX_FUZZ]) {
        printf("fuzz activated!\n");
      }
    case FX_FILTER:
      if (sf->fx_active[FX_FILTER]) {
        EnvelopeLinearInteger_reset(
            envelope_filter, BLOCKS_PER_SECOND,
            EnvelopeLinearInteger_update(envelope_filter, NULL), 5, 1.618);
      } else {
        EnvelopeLinearInteger_reset(
            envelope_filter, BLOCKS_PER_SECOND,
            EnvelopeLinearInteger_update(envelope_filter, NULL),
            global_filter_index, 1.618);
      }
      break;
    case FX_VOLUME_RAMP:
      if (sf->fx_active[FX_VOLUME_RAMP]) {
        Envelope2_reset(envelope_volume, BLOCKS_PER_SECOND,
                        Envelope2_update(envelope_volume), 0, 1.618 / 2);
      } else {
        Envelope2_reset(envelope_volume, BLOCKS_PER_SECOND,
                        Envelope2_update(envelope_volume), 1, 1.618 / 2);
      }
      break;
    case FX_TIMESTRETCH:
      if (sf->fx_active[FX_TIMESTRETCH]) {
        sel_variation_next = 1;
      } else {
        sel_variation_next = 0;
      }
      fil_current_change = true;
      break;
    default:
      break;
  }
}

void button_key_off_held(uint8_t key) { printf("off held %d\n", key); }

// triggers on ANY key off, used for 1-16 off's
void button_key_off_any(uint8_t key) {
  printf("off any %d\n", key);
  if (key > 3) {
    // 1-16 off
    // TODO: make this an option?
    if (key_total_pressed == 0) {
      dub_step_break = -1;
      //      key_do_jump(key - 4);
#ifdef INCLUDE_SINEBASS
      if (mode_buttons16 == MODE_BASS) {
        // turn off sinosc
        WaveBass_release(wavebass);
      }
#endif
    } else {
#ifdef INCLUDE_SINEBASS
      if (mode_buttons16 == MODE_BASS) {
        // find which key is on
        for (uint8_t i = 4; i < BUTTONMATRIX_BUTTONS_MAX; i++) {
          if (key_on_buttons[i] > 0) {
            WaveBass_note_on(wavebass, i - 4 + 1);
            break;
          }
        }
      }
#endif
    }
  }
}

void button_key_on_single(uint8_t key) {
  printf("on single %d\n", key);
  if (key < 4) {
  } else if (key >= 4) {
    // 1-16
    if (mode_buttons16 == MODE_JUMP) {
      // 1-16 (jump mode)
      // do jump
      debounce_quantize = 2;
      key_do_jump(key - 4);
      dub_step_break = 0;
      dub_step_divider = 0;
      dub_step_beat = beat_current;
      printf("dub_step_beat: %d\n", dub_step_beat);
      // if (toggle_chain_rec) {
      //   Chain_add_current(chain, key - 4, bpm_timer_counter);
      // }
    } else if (mode_buttons16 == MODE_MASH) {
      // 1-16 (mash mode)
      // do momentary fx
      toggle_fx(key - 4);
    }
  }
}

void button_key_on_double(uint8_t key1, uint8_t key2) {
  printf("on %d+%d\n", key1, key2);
  if (key1 == KEY_SHIFT) {
    if (key2 == KEY_A) {
      // S+A
      mode_buttons16 = MODE_JUMP;
    } else if (key2 == KEY_B) {
      // S+B
      mode_buttons16 = MODE_MASH;
      // // toggle mute
    } else if (key2 == KEY_C) {
      // S+C
      // toggle bass mode
      mode_buttons16 = MODE_BASS;
    } else {
      // S+H
      if (mode_buttons16 == MODE_JUMP) {
        // S+H (jump mode)
        // toggles fx
        toggle_fx(key2 - 4);
      } else if (mode_buttons16 == MODE_MASH) {
        // S+H (mash mode)
        // does jump
        key_do_jump(key2 - 4);
      }
    }
  } else if (key1 > 3 && key2 > 3) {
    // H+H
    if (mode_buttons16 == MODE_JUMP) {
      // retrigger
      go_retrigger_2key(key1, key2);
    }
  } else if (key1 == KEY_A) {
    // A
    if (key2 == KEY_B) {
      // A+B
      if (button_mute) {
        printf("[button_handler] button_mute off\n");
        button_mute = false;
      } else if (!button_mute) {
        printf("[button_handler] trigger button_mute\n");
        trigger_button_mute = true;
      }

      // // toggle one-shot vs classic
      // banks[sel_bank_cur]
      //     ->sample[sel_sample_cur]
      //     .snd[sel_variation]
      //     ->play_mode = (banks[sel_bank_cur]
      //                        ->sample[sel_sample_cur]
      //                        .snd[sel_variation]
      //                        ->play_mode +
      //                    1) %
      //                   3;
    } else if (key2 == KEY_C) {
      // A+C
      if (playback_stopped) {
        do_restart_playback = true;
        button_mute = false;
      } else {
        if (!button_mute) trigger_button_mute = true;
        do_stop_playback = true;
      }

    } else if (key2 > 3) {
      // A+H
      if (!key_b_sample_select) {
        sel_bank_select =
            banks_with_samples[(key2 - 4) % banks_with_samples_num];
        key_b_sample_select = true;
        printf("sel_bank_select: %d\n", sel_bank_select);
      } else {
        sel_bank_next = sel_bank_select;
        sel_sample_next = ((key2 - 4) % (banks[sel_bank_next]->num_samples));
        printf("sel_bank_next: %d\n", sel_bank_next);
        printf("sel_sample_next: %d\n", sel_sample_next);
        fil_current_change = true;
        key_b_sample_select = false;
      }
    }
  } else if (key1 == KEY_B) {
    // B
    if (key2 == KEY_A) {
      // B + A
      // toggle play sequence
      // toggle_chain_play = !toggle_chain_play;
      // if (toggle_chain_rec) {
      //   Chain_save(chain, &sync_using_sdcard);
      // }
      // if (toggle_chain_play) {
      //   Chain_load(chain, &sync_using_sdcard);
      // }
      // toggle_chain_rec = false;
      // Chain_restart(chain);
      // if (toggle_chain_play) {
      //   printf("[button_handler] sequence playback on\n");
      // } else {
      //   printf("[button_handler] sequence playback off\n");
      // }
    } else if (key2 == KEY_C) {
      // B + C

      // toggle record sequence
      // if (toggle_chain_rec) {
      //   Chain_save(chain, &sync_using_sdcard);
      // }
      // toggle_chain_rec = !toggle_chain_rec;
      // toggle_chain_play = false;
      // if (toggle_chain_rec) {
      //   Chain_clear_seq_current(chain);
      //   printf("[button_handler] sequence recording on\n");
      // } else {
      //   printf("[button_handler] sequence recording off\n");
      // }
    } else if (key2 > 3) {
      // B + H
      // update the current chain
      // Chain_set_current(chain, key2 - 4);
    }
  }
}

void button_handler(ButtonMatrix *bm) {
  if (key_total_pressed == 0) {
    key_timer++;
  }
  if (key_timer == 300 && key_pressed_num > 0) {
    printf("combo: ");
    for (uint8_t i = 0; i < key_pressed_num; i++) {
      printf("%d ", key_pressed[i]);
    }
    printf("\n");

    // combo matching
    if (key_pressed_num == 3) {
      if (key_pressed[0] == 8 && key_pressed[1] == 9 && key_pressed[2] == 8) {
        // toggle one shot mode
        if (banks[sel_bank_cur]
                ->sample[sel_sample_cur]
                .snd[sel_variation]
                ->splice_trigger == 0) {
          banks[sel_bank_cur]
              ->sample[sel_sample_cur]
              .snd[sel_variation]
              ->splice_trigger = 1;
          banks[sel_bank_cur]
              ->sample[sel_sample_cur]
              .snd[sel_variation]
              ->play_mode = PLAY_NORMAL;
        } else {
          banks[sel_bank_cur]
              ->sample[sel_sample_cur]
              .snd[sel_variation]
              ->splice_trigger = 0;
          banks[sel_bank_cur]
              ->sample[sel_sample_cur]
              .snd[sel_variation]
              ->play_mode = PLAY_SPLICE_STOP;
        }
        printf("combo: 8 9 8!!!\n");
      } else if (key_pressed[0] == 10 && key_pressed[1] == 11 &&
                 key_pressed[2] == 10) {
        printf("combo: 10 11 10!!!\n");
        if (banks[sel_bank_cur]
                ->sample[sel_sample_cur]
                .snd[sel_variation]
                ->play_mode < PLAY_SAMPLE_LOOP) {
          banks[sel_bank_cur]
              ->sample[sel_sample_cur]
              .snd[sel_variation]
              ->play_mode++;
        } else {
          banks[sel_bank_cur]
              ->sample[sel_sample_cur]
              .snd[sel_variation]
              ->play_mode = 0;
        }
        printf("play_mode: %d\n", banks[sel_bank_cur]
                                      ->sample[sel_sample_cur]
                                      .snd[sel_variation]
                                      ->play_mode);
      }
    } else if (key_pressed_num == 4) {
      if (key_pressed[0] == 12 && key_pressed[1] == 15 &&
          key_pressed[2] == 13 && key_pressed[3] == 14) {
        quadratic_resampling = !quadratic_resampling;
        if (quadratic_resampling) {
          printf("combo: change resampling to quadratic\n");
        } else {
          printf("combo: change resampling to linear\n");
        }
      } else if (key_pressed[0] == 8 && key_pressed[1] == 11 &&
                 key_pressed[2] == 10 && key_pressed[3] == 9) {
        banks[sel_bank_cur]
            ->sample[sel_sample_cur]
            .snd[sel_variation]
            ->tempo_match = !banks[sel_bank_cur]
                                 ->sample[sel_sample_cur]
                                 .snd[sel_variation]
                                 ->tempo_match;
        if (banks[sel_bank_cur]
                ->sample[sel_sample_cur]
                .snd[sel_variation]
                ->tempo_match) {
          printf("combo: enabled to tempo match mode\n");
        } else {
          printf("combo: disabled tempo match mode\n");
        }
      }
    }

    // B + H + H...
    // chain: select sequences to chain together
    if (key_pressed[0] == KEY_B) {
      uint8_t *links = malloc(sizeof(uint8_t) * (key_pressed_num - 1));
      uint16_t count = 0;
      for (uint8_t i = 0; i < key_pressed_num; i++) {
        if (key_pressed[i] - 4 >= 0) {
          links[count] = key_pressed[i] - 4;
          count++;
        }
      }
      if (count > 0) {
        // toggle_chain_rec = false;
        // Chain_link(chain, links, count);
      }
      free(links);
    }
    key_timer = 0;
    key_pressed_num = 0;
  }

#ifdef INCLUDE_BUTTONS
  // read the latest from the queue
  ButtonMatrix_read(bm);
#endif

  // check queue for buttons that turned off
  bool do_update_top = false;
  for (uint8_t i = 0; i < BUTTONMATRIX_BUTTONS_MAX; i++) {
    key_did_go_off[i] = false;
  }
  for (uint8_t i = 0; i < bm->off_num; i++) {
    key_total_pressed--;
    key_on_buttons_last[bm->off[i]] = key_on_buttons[bm->off[i]];
    key_on_buttons[bm->off[i]] = 0;
    key_did_go_off[bm->off[i]] = true;
    button_key_off_any(bm->off[i]);
    // printf("turned off %d\n", bm->off[i]);
    if (key_held_on && (bm->off[i] == key_held_num)) {
      button_key_off_held(bm->off[i]);

      key_held_on = false;
      // TODO:
      // use the last key pressed that is still held as the new hold
      if (key_total_pressed > 0) {
        uint16_t *indexes =
            sort_int16_t(key_on_buttons, BUTTONMATRIX_BUTTONS_MAX);
        key_held_on = true;
        key_held_num = indexes[BUTTONMATRIX_BUTTONS_MAX - 1];
        free(indexes);
      } else {
        key_num_presses = 0;
        for (uint8_t i = 0; i < BUTTONMATRIX_BUTTONS_MAX; i++) {
          key_on_buttons[i] = 0;
        }
      }
    } else if (key_held_on) {
      printf("off %d+%d\n", key_held_num, bm->off[i]);
    } else {
      printf("off %d\n", bm->off[i]);
    }

    if (bm->off[i] < 4) {
      do_update_top = true;
    }
  }

  // check queue for buttons that turned on
  bool key_held = false;
  for (uint8_t i = 0; i < bm->on_num; i++) {
    key_total_pressed++;
    if (!key_held_on) {
      key_held_on = true;
      key_held_num = bm->on[i];
      button_key_on_single(bm->on[i]);
    } else {
      button_key_on_double(key_held_num, bm->on[i]);
    }

#ifdef INCLUDE_SINEBASS
    if (mode_buttons16 == MODE_BASS) {
      printf("updaing sinosc\n");
      WaveBass_note_on(wavebass, bm->on[i] - 4 + 1);
    }
#endif

    // keep track of combos
    key_pressed[key_pressed_num] = bm->on[i];
    if (key_pressed_num < 100) {
      key_pressed_num++;
    }
    key_timer = 0;

    // keep track of all
    key_num_presses++;
    key_on_buttons_last[bm->on[i]] = key_on_buttons[bm->on[i]];
    key_on_buttons[bm->on[i]] = key_num_presses;
    if (bm->on[i] < 4) {
      do_update_top = true;
    } else {
      // if 3 are pressed, do retrig
      if (key_total_pressed == 3) {
        uint16_t *indexes =
            sort_int16_t(key_on_buttons, BUTTONMATRIX_BUTTONS_MAX);
        uint8_t keys[3];
        bool all_h = true;
        for (uint8_t i = 0; i < 3; i++) {
          keys[i] = indexes[BUTTONMATRIX_BUTTONS_MAX - 3 + i];
          printf("keys[%d]: %d\n", i, keys[i]);
          if (keys[i] < 4) {
            all_h = false;
          }
        }
        free(indexes);
        if (all_h) {
          // do retrigger!
          go_retrigger_3key(keys[0], keys[1], keys[2]);
        }
      }
    }
  }

  // rendering!

  // leds
  LEDS_clear(leds);
  // check debouncers
  if (DebounceUint8_active(debouncer_uint8[DEBOUNCE_UINT8_LED_BAR])) {
    // show an LED bar
    const uint8_t led_bar_ordering[32] = {
        17, 18, 17, 18, 16, 16, 19, 19, 13, 13, 14, 15, 15, 14, 12, 12,
        9,  10, 10, 9,  8,  8,  11, 5,  5,  11, 6,  6,  7,  4,  4,  7};
    const bool led_bar_brightness[32] = {
        0, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 0, 1, 1, 0, 1,
        0, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 1,
    };
    for (uint8_t i = 0;
         i < linlin_uint8_t(
                 DebounceUint8_get(debouncer_uint8[DEBOUNCE_UINT8_LED_BAR]), 10,
                 240, 0, 32);
         i++) {
      if (led_bar_brightness[i]) {
        LEDS_set(leds, led_bar_ordering[i], LED_BRIGHT);
      } else {
        LEDS_set(leds, led_bar_ordering[i], LED_DIM);
      }
    }
  } else {
    if (key_on_buttons[KEY_A] || key_did_go_off[KEY_A]) {
      if (key_total_pressed > 0) {
        LEDS_set(leds, sel_bank_next + 4, 2);
        LEDS_set(leds, sel_sample_next + 4, 3);
        LEDS_render(leds);
        return;
      } else {
        key_b_sample_select = false;
      }
    } else if (key_on_buttons[KEY_B] || key_did_go_off[KEY_B]) {
      if (key_total_pressed == 1) {
        // for (uint8_t i = 0; i < 16; i++) {
        //   if (Chain_has_data(chain, i)) {
        //     LEDS_set(leds, i + 4, 1);
        //   }
        // }
        // LEDS_set(leds, Chain_get_current(chain) + 4, 3);
      }
    }
    if (mode_buttons16 == MODE_MASH) {
      LEDS_set(leds, 2, LED_BRIGHT);
    } else if (mode_buttons16 == MODE_JUMP) {
      LEDS_set(leds, 1, LED_BRIGHT);
    } else if (mode_buttons16 == MODE_BASS) {
      LEDS_set(leds, 3, LED_BRIGHT);
    }
    if (mode_buttons16 == MODE_MASH || mode_buttons16 == MODE_JUMP) {
      if (sel_variation == 0) {
        LEDS_set(leds, beat_current % 16 + 4, LED_DIM);
      } else {
        LEDS_set(leds,
                 linlin(phases[0], 0,
                        banks[sel_bank_cur]
                            ->sample[sel_sample_cur]
                            .snd[sel_variation]
                            ->size,
                        0,
                        banks[sel_bank_cur]
                            ->sample[sel_sample_cur]
                            .snd[sel_variation]
                            ->slice_num) %
                         16 +
                     4,
                 LED_DIM);
      }
    }
    if (mode_buttons16 == MODE_MASH ||
        (mode_buttons16 == MODE_JUMP && key_on_buttons[KEY_SHIFT])) {
      for (uint8_t i = 0; i < 16; i++) {
        if (sf->fx_active[i]) {
          LEDS_set(leds, i + 4, LED_BRIGHT);
        }
      }
    }
  }
  for (uint8_t i = 0; i < 20; i++) {
    if (key_on_buttons[i] > 0) {
      LEDS_set(leds, i, LED_BRIGHT);
    }
  }

  if (playback_stopped) {
    LEDS_set(leds, 1, LED_BRIGHT);
    LEDS_set(leds, 3, LED_BLINK);
  } else if (button_mute) {
    LEDS_set(leds, 1, LED_BRIGHT);
    LEDS_set(leds, 2, LED_BLINK);
  }
  LEDS_render(leds);
}
