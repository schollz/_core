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
#define KEY_A 0
#define KEY_B 1
#define KEY_C 2
#define KEY_D 3

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
bool KEY_C_sample_select = false;

bool button_is_pressed(uint8_t key) { return key_on_buttons[key] > 0; }

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
  printf("[button_handler] retrigger 3key: %d %d %d\n", key1, key2, key3);
  debounce_quantize = 0;
  retrig_vol = 1.0;
  retrig_pitch = PITCH_VAL_MID;
  retrig_pitch_change = 0;
  retrig_beat_num = 0;
  key3_activated = true;
  key3_pressed_keys[0] = key1;
  key3_pressed_keys[1] = key2;
  key3_pressed_keys[2] = key3;

  // retrig_first = true;
  // retrig_beat_num = key2 + 4;
  // retrig_timer_reset = 96 / key3;
  // float total_time = (float)(retrig_beat_num * retrig_timer_reset * 60) /
  //                    (float)(96 * sf->bpm_tempo);
  // retrig_vol_step = 1.0 / ((float)retrig_beat_num);
  // printf("retrig_beat_num=%d,retrig_timer_reset=%d,total_time=%2.3fs\n",
  //        retrig_beat_num, retrig_timer_reset, total_time);
  // retrig_ready = true;
}

void go_retrigger_2key(uint8_t key1, uint8_t key2) {
  uint16_t retrig_times[16] = {
      4, 8, 16, 20, 24, 32, 48, 54, 64, 72, 96, 104, 112, 128, 144, 192,
  };
  debounce_quantize = 0;
  retrig_first = true;
  retrig_beat_num = random_integer_in_range(8, 24);
  retrig_timer_reset = retrig_times[key2 - 4];
  // 96 * random_integer_in_range(1, 6) / random_integer_in_range(2, 12);
  float total_time = (float)(retrig_beat_num * retrig_timer_reset * 60) /
                     (float)(96 * sf->bpm_tempo);
  if (total_time > 5.0f) {
    total_time = total_time / 2;
    retrig_timer_reset = retrig_timer_reset / 2;
  }
  // if (total_time > 5.0f) {
  //   total_time = total_time / 2;
  //   retrig_beat_num = retrig_beat_num / 2;
  //   if (retrig_beat_num == 0) {
  //     retrig_beat_num = 1;
  //   }
  // }
  // if (total_time < 0.5f) {
  //   total_time = total_time * 2;
  //   retrig_beat_num = retrig_beat_num * 2;
  //   if (retrig_beat_num == 0) {
  //     retrig_beat_num = 1;
  //   }
  // }
  // if (total_time < 0.5f) {
  //   total_time = total_time * 2;
  //   retrig_beat_num = retrig_beat_num * 2;
  //   if (retrig_beat_num == 0) {
  //     retrig_beat_num = 1;
  //   }
  // }
  retrig_vol_step = 1.0 / ((float)retrig_beat_num);
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
    // LEDS_set(leds, LED_BASE_FACE, KEY_B, 2 * (1 - mode_buttons16));
    // LEDS_set(leds, LED_BASE_FACE, KEY_C, 2 * mode_mute);
    // LEDS_set(leds, LED_BASE_FACE, KEY_D, 2 * mode_play);
  }
}

// toggle the fx
void toggle_fx(uint8_t fx_num) {
  sf->fx_active[fx_num] = !sf->fx_active[fx_num];
  if (sequencerhandler[1].recording) {
    if (sf->fx_active[fx_num]) {
      Sequencer_add(sf->sequencers[1][sf->sequence_sel[1]], fx_num,
                    bpm_timer_counter);
    } else {
      Sequencer_add(sf->sequencers[1][sf->sequence_sel[1]], fx_num + 16,
                    bpm_timer_counter);
    }
  }
  update_fx(fx_num);
}

void button_key_off_held(uint8_t key) { printf("off held %d\n", key); }

// triggers on ANY key off, used for 1-16 off's
void button_key_off_any(uint8_t key) {
  printf("off any %d\n", key);
  if (key_total_pressed < 3) {
    key3_activated = false;
  }
  if (key > 3) {
    // 1-16 off
    // TODO: make this an option?
    if (key_total_pressed == 0) {
      if (mode_hands_on_unmute) {
        if (!button_mute) {
          printf("[button_handler] mode_hands_on_unmute -> mute\n");
          trigger_button_mute = true;
        }
      }
      dub_step_break = -1;
      //      key_do_jump(key - 4);
#ifdef INCLUDE_SINEBASS
      if (mode_buttons16 == MODE_BASS) {
        // turn off sinosc
        WaveBass_release(wavebass);
        if (sequencerhandler[2].recording) {
          // TODO: rests create a problem with the end of the sequence
          // Sequencer_add(sf->sequencers[2][sf->sequence_sel[2]], 17,
          //               bpm_timer_counter);
        }
      }
#endif
    } else {
#ifdef INCLUDE_SINEBASS
      if (mode_buttons16 == MODE_BASS) {
        // find which key is on
        for (uint8_t i = 4; i < BUTTONMATRIX_BUTTONS_MAX; i++) {
          if (key_on_buttons[i] > 0) {
            uint8_t octave = 0;
            if (key_on_buttons[0] > 0) {
              octave = 12;
            }

            WaveBass_note_on(wavebass, octave + i - 4);
            if (sequencerhandler[2].recording) {
              Sequencer_add(sf->sequencers[2][sf->sequence_sel[2]],
                            octave + i - 4, bpm_timer_counter);
            }

            break;
          }
        }
      }
#endif
    }
  }
}

uint32_t tap_tempo_last = 0;
uint8_t tap_tempo_hits = 0;

void button_key_on_single(uint8_t key) {
  printf("on single %d\n", key);
  if (key < 4) {
    if (key == KEY_A) {
    }
  } else if (key >= 4) {
    if (mode_hands_on_unmute) {
      if (button_mute) {
        printf("[button_handler] mode_hands_on_unmute unmute\n");
        button_mute = false;
        trigger_button_mute = false;
      }
    }
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
  if (key1 == KEY_A) {
    if (key2 == KEY_B) {
      // S+A
      mode_buttons16 = MODE_JUMP;
      uint16_t val = TapTempo_tap(taptempo);
      if (val > 0) {
        uint32_t current_time = time_us_32();
        if (tap_tempo_last == 0) {
          tap_tempo_last = current_time;
        }
        if (current_time - tap_tempo_last < 1000000) {
          tap_tempo_hits++;
          if (tap_tempo_hits > 2) {
            printf("tap bpm -> %d\n", val);
            sf->bpm_tempo = val;
            DebounceDigits_set(debouncer_digits, sf->bpm_tempo, 200);
          }
        } else {
          tap_tempo_hits = 0;
        }
        tap_tempo_last = current_time;
      }
    } else if (key2 == KEY_C) {
      // S+B
      mode_buttons16 = MODE_MASH;
      // // toggle mute
    } else if (key2 == KEY_D) {
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
  } else if (key1 == KEY_B) {
    // A
    if (key2 == KEY_C) {
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
      //     .snd[FILEZERO]
      //     ->play_mode = (banks[sel_bank_cur]
      //                        ->sample[sel_sample_cur]
      //                        .snd[FILEZERO]
      //                        ->play_mode +
      //                    1) %
      //                   3;
    } else if (key2 == KEY_D) {
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
      if (!KEY_C_sample_select) {
        sel_bank_select =
            banks_with_samples[(key2 - 4) % banks_with_samples_num];
        KEY_C_sample_select = true;
        printf("sel_bank_select: %d\n", sel_bank_select);
      } else {
        sel_bank_next = sel_bank_select;
        sel_sample_next = ((key2 - 4) % (banks[sel_bank_next]->num_samples));
        printf("sel_bank_next: %d\n", sel_bank_next);
        printf("sel_sample_next: %d\n", sel_sample_next);
        fil_current_change = true;
        KEY_C_sample_select = false;
      }
    }
  } else if (key1 == KEY_C) {
    // B
    if (key2 == KEY_B) {
      // B + A
      // toggle play sequence
      if (sequencerhandler[mode_buttons16].recording) {
        sequencerhandler[mode_buttons16].recording = false;
      }
      sequencerhandler[mode_buttons16].playing =
          !sequencerhandler[mode_buttons16].playing;

      if (sequencerhandler[mode_buttons16].playing) {
        printf("[button_handler] sequence %d playing on\n", mode_buttons16);
        if (Sequencer_has_data(
                sf->sequencers[mode_buttons16]
                              [sf->sequence_sel[mode_buttons16]])) {
          Sequencer_play(
              sf->sequencers[mode_buttons16][sf->sequence_sel[mode_buttons16]],
              true);
        } else {
          printf("[button_handler] sequence %d has no data\n", mode_buttons16);
          sequencerhandler[mode_buttons16].playing = false;
        }
      } else {
        printf("[button_handler] sequence %d playing off\n", mode_buttons16);
        Sequencer_stop(
            sf->sequencers[mode_buttons16][sf->sequence_sel[mode_buttons16]]);
        if (mode_buttons16 == MODE_BASS) {
#ifdef INCLUDE_SINEBASS
          WaveBass_release(wavebass);
#endif
        }
      }
    } else if (key2 == KEY_D) {
      // B + C

      // toggle record sequence
      if (sequencerhandler[mode_buttons16].playing) {
        sequencerhandler[mode_buttons16].playing = false;
      }
      sequencerhandler[mode_buttons16].recording =
          !sequencerhandler[mode_buttons16].recording;
      if (sequencerhandler[mode_buttons16].recording) {
        // todo [0] should be which sequencer is currently on
        Sequencer_clear(
            sf->sequencers[mode_buttons16][sf->sequence_sel[mode_buttons16]]);
        printf("[button_handler] sequence %d recording on\n", mode_buttons16);
      } else {
        printf("[button_handler] sequence %d recording off\n", mode_buttons16);
      }

    } else if (key2 > 3) {
      // B + H
      // change the current pattern if it exists
      uint8_t prev_sequence = sf->sequence_sel[mode_buttons16];
      sf->sequence_sel[mode_buttons16] = key2 - 4;
      if (sequencerhandler[mode_buttons16].recording) {
        sequencerhandler[mode_buttons16].recording = false;
      }
      if (sequencerhandler[mode_buttons16].playing) {
        Sequencer_stop(sf->sequencers[mode_buttons16][prev_sequence]);
        if (Sequencer_has_data(
                sf->sequencers[mode_buttons16]
                              [sf->sequence_sel[mode_buttons16]])) {
          Sequencer_play(
              sf->sequencers[mode_buttons16][sf->sequence_sel[mode_buttons16]],
              true);
        }
      }
    }
  } else if (key1 == KEY_D) {
    // D
    if (key2 == KEY_B) {
      // D+B
      // do load
      printf("[button_handler] loading %d to sd card\n", savefile_current);
      savefile_do_load();
    } else if (key2 == KEY_C) {
      // D+C
      // do save
      printf("[button_handler] saving %d to sd card\n", savefile_current);
      SaveFile_save(sf, &sync_using_sdcard, savefile_current);
      // load prevoius file
      printf("[button_handler] loading %s again\n", fil_current_name);
      f_open(&fil_current, fil_current_name, FA_READ);
      savefile_has_data[savefile_current] = true;
    } else if (key2 == KEY_A) {
      // D+A
    } else {
      // D+H
      savefile_current = key2 - 4;
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

    if (key_pressed_num >= 3) {
      bool do_merge = key_pressed[0] == KEY_C;
      for (uint8_t i = 1; i < key_pressed_num; i++) {
        if (key_pressed[i] < 4) {
          do_merge = false;
        }
      }
      if (do_merge) {
        uint8_t merge_sequence_num = key_pressed[1] - 4;
        if (!Sequencer_has_data(
                sf->sequencers[mode_buttons16][merge_sequence_num])) {
          printf("[button_handler] merging sequences into sequence %d: \n",
                 merge_sequence_num);
          // copy the first sequence into the empty slot
          Sequencer_copy(sf->sequencers[mode_buttons16][key_pressed[2] - 4],
                         sf->sequencers[mode_buttons16][merge_sequence_num]);
          // merge the rest into that slot
          for (uint8_t i = 3; i < key_pressed_num; i++) {
            printf("merging %d\n", key_pressed[i] - 4);
            Sequencer *merged = Sequencer_merge(
                sf->sequencers[mode_buttons16][merge_sequence_num],
                sf->sequencers[mode_buttons16][key_pressed[i] - 4]);
            Sequencer_copy(merged,
                           sf->sequencers[mode_buttons16][merge_sequence_num]);
            free(merged);
          }
          Sequencer_print(sf->sequencers[mode_buttons16][merge_sequence_num]);
          sf->sequence_sel[mode_buttons16] = merge_sequence_num;
          sequencerhandler[mode_buttons16].playing = true;
          Sequencer_play(sf->sequencers[mode_buttons16][merge_sequence_num],
                         true);
        }
      }
    }

    // combo matching
    if (key_pressed_num == 3) {
      if (key_pressed[0] == 8 && key_pressed[1] == 9 && key_pressed[2] == 8) {
        // toggle one shot mode
        if (banks[sel_bank_cur]
                ->sample[sel_sample_cur]
                .snd[FILEZERO]
                ->one_shot) {
          printf("toggle one shot OFF ");
          banks[sel_bank_cur]->sample[sel_sample_cur].snd[FILEZERO]->one_shot =
              false;
          banks[sel_bank_cur]->sample[sel_sample_cur].snd[FILEZERO]->play_mode =
              PLAY_NORMAL;
          DebounceDigits_setText(debouncer_digits, "ONESHOT OOFFF", 200);
        } else {
          printf("toggle one shot ON ");
          banks[sel_bank_cur]->sample[sel_sample_cur].snd[FILEZERO]->one_shot =
              true;
          banks[sel_bank_cur]->sample[sel_sample_cur].snd[FILEZERO]->play_mode =
              PLAY_SPLICE_STOP;
          DebounceDigits_setText(debouncer_digits, "ONESHOT OONN", 200);
        }
        printf("combo: 8 9 8!!!\n");
      } else if (key_pressed[0] == 10 && key_pressed[1] == 11 &&
                 key_pressed[2] == 10) {
        printf("combo: 10 11 10!!!\n");
        if (banks[sel_bank_cur]
                ->sample[sel_sample_cur]
                .snd[FILEZERO]
                ->play_mode < PLAY_SAMPLE_LOOP) {
          banks[sel_bank_cur]
              ->sample[sel_sample_cur]
              .snd[FILEZERO]
              ->play_mode++;
        } else {
          banks[sel_bank_cur]->sample[sel_sample_cur].snd[FILEZERO]->play_mode =
              0;
        }
        printf("play_mode: %d\n", banks[sel_bank_cur]
                                      ->sample[sel_sample_cur]
                                      .snd[FILEZERO]
                                      ->play_mode);
      } else if (key_pressed[0] == 4 && key_pressed[1] == 5 &&
                 key_pressed[2] == 4) {
        printf("toggling variable splice\n");
        banks[sel_bank_cur]
            ->sample[sel_sample_cur]
            .snd[FILEZERO]
            ->splice_variable = !banks[sel_bank_cur]
                                     ->sample[sel_sample_cur]
                                     .snd[FILEZERO]
                                     ->splice_variable;
      }
    } else if (key_pressed_num == 4) {
      if (key_pressed[0] == 4 && key_pressed[1] == 5 && key_pressed[2] == 6 &&
          key_pressed[3] == 7) {
        sf->stay_in_sync = !sf->stay_in_sync;
        printf("toggling in sync mode: %d\n", sf->stay_in_sync);
        if (sf->stay_in_sync) {
          DebounceDigits_setText(debouncer_digits, "LOCK OONN", 200);
        } else {
          DebounceDigits_setText(debouncer_digits, "LOCK OOFFFF", 200);
        }
      } else if (key_pressed[0] == 7 && key_pressed[1] == 10 &&
                 key_pressed[2] == 13 && key_pressed[3] == 16) {
        printf("combo: 7 10 13 16!!!\n");
        only_play_kicks = !only_play_kicks;
        printf("only_play_kicks: %d\n", only_play_kicks);
        if (only_play_kicks) {
          DebounceDigits_setText(debouncer_digits, "KICKS", 200);
        } else {
          DebounceDigits_setText(debouncer_digits, "NORM", 200);
        }
      } else if (key_pressed[0] == 16 && key_pressed[1] == 13 &&
                 key_pressed[2] == 10 && key_pressed[3] == 6) {
        only_play_snares = !only_play_snares;
        printf("only_play_snares: %d\n", only_play_snares);
        if (only_play_snares) {
          DebounceDigits_setText(debouncer_digits, "SNARES", 200);
        } else {
          DebounceDigits_setText(debouncer_digits, "NORM", 200);
        }
      } else if (key_pressed[0] == 12 && key_pressed[1] == 15 &&
                 key_pressed[2] == 13 && key_pressed[3] == 14) {
        quadratic_resampling = !quadratic_resampling;
        if (quadratic_resampling) {
          printf("combo: change resampling to quadratic\n");
          DebounceDigits_setText(debouncer_digits, "QUAD", 200);
        } else {
          printf("combo: change resampling to linear\n");
          DebounceDigits_setText(debouncer_digits, "LINEAR", 200);
        }
      } else if (key_pressed[0] == 8 && key_pressed[1] == 9 &&
                 key_pressed[2] == 10 && key_pressed[3] == 11) {
        banks[sel_bank_cur]->sample[sel_sample_cur].snd[FILEZERO]->tempo_match =
            !banks[sel_bank_cur]
                 ->sample[sel_sample_cur]
                 .snd[FILEZERO]
                 ->tempo_match;
        if (banks[sel_bank_cur]
                ->sample[sel_sample_cur]
                .snd[FILEZERO]
                ->tempo_match) {
          printf("combo: enabled to tempo match mode\n");
          DebounceDigits_setText(debouncer_digits, "MATCH OONN", 200);
        } else {
          printf("combo: disabled tempo match mode\n");
          DebounceDigits_setText(debouncer_digits, "MATCH OOFFF", 200);
        }

      } else if (key_pressed[0] == 12 && key_pressed[1] == 13 &&
                 key_pressed[2] == 14 && key_pressed[3] == 15) {
        do_retrig_pitch_changes = !do_retrig_pitch_changes;
        printf("combo: enabled to pitch retrig mode: %d\n",
               do_retrig_pitch_changes);
        if (do_retrig_pitch_changes) {
          DebounceDigits_setText(debouncer_digits, "PIT OONN", 200);
        } else {
          DebounceDigits_setText(debouncer_digits, "PIT OOFF", 200);
        }
      }
    } else if (key_pressed_num == 8) {
      if (key_pressed[0] == 16 && key_pressed[1] == 12 && key_pressed[2] == 8 &&
          key_pressed[3] == 4 && key_pressed[4] == 18 && key_pressed[5] == 14 &&
          key_pressed[6] == 10 && key_pressed[7] == 6) {
        clock_out_do = !clock_out_do;
        if (clock_out_do) {
          printf("[button_handler]: combo: clock out enabled\n");
          DebounceDigits_setText(debouncer_digits, "SYNC OONN", 200);
        } else {
          printf("[button_handler]: combo: clock out disabled\n");
          DebounceDigits_setText(debouncer_digits, "SYNC OOFFF", 200);
        }
      }
    }

    // B + H + H...
    // chain: select sequences to chain together
    if (key_pressed[0] == KEY_C) {
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
    if (mode_buttons16 == MODE_BASS && bm->on[i] > 3) {
      uint8_t octave = 0;
      if (key_on_buttons[0] > 0) {
        octave = 12;
      }
      printf("playing note: %d\n", octave + bm->on[i] - 4);
      WaveBass_note_on(wavebass, octave + bm->on[i] - 4);
      if (sequencerhandler[2].recording) {
        Sequencer_add(sf->sequencers[2][sf->sequence_sel[2]],
                      octave + bm->on[i] - 4, bpm_timer_counter);
      }
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

  // show the sequencer state when pressing the key down
  if (key_pressed_num > 0 && key_pressed[0] == KEY_C && key_on_buttons[KEY_C]) {
    LEDS_set(leds, KEY_C, LED_BRIGHT);
    for (uint8_t i = 0; i < 16; i++) {
      // TODO blink the current sequence
      if (sf->sequence_sel[mode_buttons16] == i) {
        if (Sequencer_has_data(sf->sequencers[mode_buttons16][i])) {
          LEDS_set(leds, i + 4, LED_BRIGHT);
        } else {
          LEDS_set(leds, i + 4, LED_BLINK);
        }
      } else if (Sequencer_has_data(sf->sequencers[mode_buttons16][i])) {
        LEDS_set(leds, i + 4, LED_DIM);
      } else {
        LEDS_set(leds, i + 4, 0);
      }
    }

    for (uint8_t i = 0; i < 3; i++) {
      if (sequencerhandler[i].playing) {
        LEDS_set(leds, 0, 0);
        LEDS_set(leds, 1, LED_BLINK);
        LEDS_set(leds, 2, LED_BRIGHT);
        LEDS_set(leds, 3, 0);
      } else if (sequencerhandler[i].recording) {
        LEDS_set(leds, 0, 0);
        LEDS_set(leds, 1, 0);
        LEDS_set(leds, 2, LED_BRIGHT);
        LEDS_set(leds, 3, LED_BLINK);
      }
    }
    LEDS_render(leds);
    return;
  }

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
    LEDS_render(leds);
    return;
  } else if (DebounceUint8_active(
                 debouncer_uint8[DEBOUNCE_UINT8_LED_SPIRAL1])) {
    // show an LED bar
    const uint8_t led_bar_ordering[32] = {
        10, 14, 13, 9, 5, 6, 7, 11, 15, 19, 18, 17, 16, 12, 8, 4,
        10, 14, 13, 9, 5, 6, 7, 11, 15, 19, 18, 17, 16, 12, 8, 4};
    const bool led_bar_brightness[32] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    };
    for (uint8_t i = 0;
         i < linlin_uint8_t(
                 DebounceUint8_get(debouncer_uint8[DEBOUNCE_UINT8_LED_SPIRAL1]),
                 10, 240, 0, 32);
         i++) {
      if (led_bar_brightness[i]) {
        LEDS_set(leds, led_bar_ordering[i], LED_BRIGHT);
      } else {
        LEDS_set(leds, led_bar_ordering[i], LED_DIM);
      }
    }
    LEDS_render(leds);
    return;
  } else if (DebounceUint8_active(
                 debouncer_uint8[DEBOUNCE_UINT8_LED_RANDOM1])) {
    // show an LED bar
    const uint8_t led_bar_ordering[32] = {

        18, 11, 8, 4, 16, 7, 13, 19, 10, 5, 14, 15, 17, 12, 6,  9,
        13, 14, 6, 9, 16, 8, 18, 12, 11, 7, 10, 17, 15, 5,  19, 4};
    const bool led_bar_brightness[32] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    };
    for (uint8_t i = 0;
         i < linlin_uint8_t(
                 DebounceUint8_get(debouncer_uint8[DEBOUNCE_UINT8_LED_RANDOM1]),
                 10, 240, 0, 32);
         i++) {
      if (led_bar_brightness[i]) {
        LEDS_set(leds, led_bar_ordering[i], LED_BRIGHT);
      } else {
        LEDS_set(leds, led_bar_ordering[i], LED_DIM);
      }
    }
    LEDS_render(leds);
    return;
  } else if (DebounceUint8_active(
                 debouncer_uint8[DEBOUNCE_UINT8_LED_RANDOM2])) {
    // show an LED bar
    const uint8_t led_bar_ordering[32] = {
        7,  16, 18, 14, 11, 5,  9, 12, 13, 15, 6,  4,  10, 8, 19, 17,
        18, 8,  7,  10, 9,  14, 6, 19, 11, 12, 16, 15, 17, 5, 4,  13};
    const bool led_bar_brightness[32] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    };
    for (uint8_t i = 0;
         i < linlin_uint8_t(
                 DebounceUint8_get(debouncer_uint8[DEBOUNCE_UINT8_LED_RANDOM2]),
                 10, 240, 0, 32);
         i++) {
      if (led_bar_brightness[i]) {
        LEDS_set(leds, led_bar_ordering[i], LED_BRIGHT);
      } else {
        LEDS_set(leds, led_bar_ordering[i], LED_DIM);
      }
    }
    LEDS_render(leds);
    return;
  } else if (DebounceUint8_active(
                 debouncer_uint8[DEBOUNCE_UINT8_LED_DIAGONAL])) {
    // show an LED bar
    const uint8_t led_bar_ordering[32] = {
        16, 12, 17, 18, 8, 13, 4, 19, 9, 14, 5, 15, 10, 6, 11, 7,
        16, 12, 17, 18, 8, 13, 4, 19, 9, 14, 5, 15, 10, 6, 11, 7,
    };
    const bool led_bar_brightness[32] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    };
    for (uint8_t i = 0;
         i < linlin_uint8_t(DebounceUint8_get(
                                debouncer_uint8[DEBOUNCE_UINT8_LED_DIAGONAL]),
                            10, 240, 0, 32);
         i++) {
      if (led_bar_brightness[i]) {
        LEDS_set(leds, led_bar_ordering[i], LED_BRIGHT);
      } else {
        LEDS_set(leds, led_bar_ordering[i], LED_DIM);
      }
    }
    LEDS_render(leds);
    return;
  } else if (DebounceDigits_active(debouncer_digits)) {
    char digit = DebounceDigits_get(debouncer_digits);

    for (int i = 0; i < sizeof(led_text_5x4) / sizeof(led_text_5x4[0]); i++) {
      if (led_text_5x4[i].character == digit) {
        int led = 0;
        for (int j = 0; j < 5; j++) {
          // print out 0 or 1
          for (int k = 0; k < 4; k++) {
            bool is_one = (led_text_5x4[i].dots[j] >> (3 - k)) & 1;
            if (is_one) {
              LEDS_set(leds, led, LED_BRIGHT);
            } else {
              LEDS_set(leds, led, 0);
            }
            led++;
          }
        }
        break;
      }
    }
    LEDS_render(leds);
    return;
  } else if (DebounceUint8_active(debouncer_uint8[DEBOUNCE_UINT8_LED_WALL])) {
    // show an LED bar
    const uint8_t led_bar_ordering[32] = {
        16, 12, 8,  4, 4, 8,  12, 16, 17, 13, 9,  5,  5,  9,  13, 17,
        18, 14, 10, 6, 6, 10, 14, 18, 7,  11, 15, 19, 19, 15, 11, 7};
    const bool led_bar_brightness[32] = {
        0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1,
        0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1,
    };
    for (uint8_t i = 0;
         i < linlin_uint8_t(
                 DebounceUint8_get(debouncer_uint8[DEBOUNCE_UINT8_LED_WALL]),
                 10, 240, 0, 32);
         i++) {
      if (led_bar_brightness[i]) {
        LEDS_set(leds, led_bar_ordering[i], LED_BRIGHT);
      } else {
        LEDS_set(leds, led_bar_ordering[i], LED_DIM);
      }
    }
    LEDS_render(leds);
    return;
  } else if (key_pressed_num > 0 && key_on_buttons[KEY_D]) {
    // show the current save file is pressed
    LEDS_set(leds, KEY_D, LED_BRIGHT);
    for (uint8_t i = 0; i < 16; i++) {
      if (savefile_has_data[i]) {
        if (savefile_current == i) {
          LEDS_set(leds, i + 4, LED_BRIGHT);
        } else {
          LEDS_set(leds, i + 4, LED_DIM);
        }
      } else {
        if (savefile_current == i) {
          LEDS_set(leds, i + 4, LED_BLINK);
        } else {
          LEDS_set(leds, i + 4, 0);
        }
      }
    }
    LEDS_render(leds);
    return;
  } else {
    if (key_on_buttons[KEY_B] || key_did_go_off[KEY_B]) {
      if (key_total_pressed > 0) {
        // illuminate which banks have samples
        for (uint8_t i = 0; i < 16; i++) {
          if (banks_with_samples[i] > 0) {
            LEDS_set(leds, i + 4, LED_DIM);
          }
        }
        LEDS_set(leds, sel_bank_next + 4, 2);
        LEDS_set(leds, sel_sample_next + 4, 3);
        LEDS_render(leds);
        return;
      } else {
        KEY_C_sample_select = false;
      }
    } else if (key_on_buttons[KEY_C] || key_did_go_off[KEY_C]) {
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
      LEDS_set(leds, 0, LED_BRIGHT);
      LEDS_set(leds, 1, 0);
      LEDS_set(leds, 2, LED_BLINK);
      LEDS_set(leds, 3, 0);
    } else if (mode_buttons16 == MODE_JUMP) {
      LEDS_set(leds, 0, LED_BRIGHT);
      LEDS_set(leds, 1, LED_BLINK);
      LEDS_set(leds, 2, 0);
      LEDS_set(leds, 3, 0);
    } else if (mode_buttons16 == MODE_BASS) {
      LEDS_set(leds, 0, LED_BRIGHT);
      LEDS_set(leds, 1, 0);
      LEDS_set(leds, 2, 0);
      LEDS_set(leds, 3, LED_BLINK);
    }
    if (mode_buttons16 == MODE_MASH || mode_buttons16 == MODE_JUMP) {
      if (sel_variation == 0) {
        LEDS_set(leds, beat_current % 16 + 4, LED_DIM);
      } else {
        LEDS_set(
            leds,
            linlin(
                phases[0], 0,
                banks[sel_bank_cur]->sample[sel_sample_cur].snd[FILEZERO]->size,
                0,
                banks[sel_bank_cur]
                    ->sample[sel_sample_cur]
                    .snd[FILEZERO]
                    ->slice_num) %
                    16 +
                4,
            LED_DIM);
      }
    }
    if (mode_buttons16 == MODE_MASH ||
        (mode_buttons16 == MODE_JUMP && key_on_buttons[KEY_A])) {
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
    LEDS_set(leds, 0, 0);
    LEDS_set(leds, 1, LED_BRIGHT);
    LEDS_set(leds, 2, 0);
    LEDS_set(leds, 3, LED_BLINK);
  } else if (button_mute) {
    LEDS_set(leds, 0, 0);
    LEDS_set(leds, 1, LED_BRIGHT);
    LEDS_set(leds, 2, LED_BLINK);
    LEDS_set(leds, 3, 0);
  }

  LEDS_render(leds);
}
