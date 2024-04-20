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
#include "clockhandling.h"
#include "midi_comm_callback.h"

void printStringWithDelay(char *str) {
  int len = strlen(str);
  for (int i = 0; i < len; i++) {
    char currentChar = str[i];
    for (int j = 0; j < sizeof(led_text_5x4) / sizeof(led_text_5x4[0]); j++) {
      if (led_text_5x4[j].character == currentChar) {
        int led = 0;
        for (int row = 0; row < 5; row++) {
          for (int col = 0; col < 4; col++) {
            bool is_one = (led_text_5x4[j].dots[row] >> (3 - col)) & 1;
            if (is_one) {
              LEDS_set(leds, led, LED_BRIGHT);
            } else {
              LEDS_set(leds, led, 0);
            }
            led++;
          }
          printf("\n");
        }
        printf("\n");
        LEDS_render(leds);
        if (currentChar == '.') {
          sleep_ms(50);
        } else {
          sleep_ms(200);
        }
        break;
      }
    }
  }
}

void clear_debouncers() {
  for (uint8_t i = 0; i < DEBOUNCE_UINT8_NUM; i++) {
    DebounceUint8_clear(debouncer_uint8[i]);
  }
  DebounceDigits_clear(debouncer_digits);
}

void input_handling() {
  printf("core1 running!\n");
  // flash bad signs
  while (!fil_is_open) {
    printf("waiting to start\n");
    sleep_ms(10);
  }
  LEDS_clear(leds);
  LEDS_render(leds);

#ifdef BTN_COL_START
  ButtonMatrix *bm;
  // initialize button matrix
  bm = ButtonMatrix_create(BTN_ROW_START, BTN_COL_START);
#endif

#ifdef INCLUDE_MIDI
  // initialize the midi
  for (uint8_t i = 0; i < 3; i++) {
    midiout[i] = MidiOut_malloc(i, false);
  }
  for (uint8_t i = 3; i < MIDIOUTS; i++) {
    midiout[i] = MidiOut_malloc(i, true);
  }
#endif

  printf("entering while loop\n");

  uint pressed2 = 0;
  uint8_t new_vol;
  //   (
  // a=Array.fill(72,{ arg i;
  //   (i+60).midicps.round.asInteger
  // });
  // a.postln;
  // )
  ClockInput *clockinput =
      ClockInput_create(CLOCK_INPUT_GPIO, clock_handling_up,
                        clock_handling_down, clock_handling_start);

  FilterExp *adcs[3];
  int adc_last[3] = {0, 0, 0};
  int adc_debounce[3] = {0, 0, 0};
  const int adc_threshold = 200;
  const int adc_debounce_max = 250;
  uint16_t adc_startup = 300;
  // TODO add debounce for the adc detection
  for (uint8_t i = 0; i < 3; i++) {
    adcs[i] = FilterExp_create(10);
  }

  uint8_t debounce_beat_repeat = 0;

  // debug test
  printStringWithDelay("zv2.3.2");

  // print to screen
  printf("version=v2.3.2\n");

  // initialize the resonsant filter
  global_filter_index = 12;
  for (uint8_t channel = 0; channel < 2; channel++) {
    ResonantFilter_setFilterType(resFilter[channel], 0);
    ResonantFilter_setFc(resFilter[channel], global_filter_index);
  }
  global_filter_index = resonantfilter_fc_max;
  for (uint8_t channel = 0; channel < 2; channel++) {
    ResonantFilter_setFilterType(resFilter[channel], 0);
    ResonantFilter_setFc(resFilter[channel], global_filter_index);
  }
#ifdef INCLUDE_MIDI
  tusb_init();
#endif

  while (1) {
#ifdef INCLUDE_MIDI
    tud_task();
    midi_comm_task(midi_comm_callback_fn);
#endif

    // if in startup deduct
    if (adc_startup > 0) {
      adc_startup--;
    }

    // check for input
    int char_input = getchar_timeout_us(10);
    if (char_input >= 0) {
      if (char_input == 118) {
        printf("version=v2.3.2\n");
      }
    }

#ifdef BTN_COL_START
    // button handler
    button_handler(bm);
#endif

    if (MessageSync_hasMessage(messagesync)) {
      MessageSync_print(messagesync);
      MessageSync_clear(messagesync);
    }

    // check to see if the probability knobs are activated for the fx
    if (clock_did_activate) {
      clock_did_activate = false;
      for (uint8_t i = 0; i < 16; i++) {
        if (sf->fx_param[i][2] > 0) {
          if (sf->fx_active[i]) {
            if (random_integer_in_range(0, 96) <
                probability_max_values_off[sf->fx_param[i][2] >> 4]) {
              toggle_fx(i);
              printf("[zeptocore] random fx: %d %d\n", i, sf->fx_active[i]);
            }
          } else {
            if (random_integer_in_range(0, 96) <
                probability_max_values[sf->fx_param[i][2] >> 4]) {
              toggle_fx(i);
              // TODO: also randomize the parameters?
              printf("[zeptocore] random fx: %d %d\n", i, sf->fx_active[i]);
            }
          }
        }
      }
    }

#ifdef PRINT_SDCARD_TIMING
    // random stuff
    if (random_integer_in_range(1, 10000) < 10) {
      // printf("random retrig\n");
      key_do_jump(random_integer_in_range(0, 15));
    } else if (random_integer_in_range(1, 10000) < 5) {
      // printf("random retrigger\n");
      go_retrigger_2key(random_integer_in_range(0, 15),
                        random_integer_in_range(0, 15));
    }
#endif

    if (random_integer_in_range(1, 1000000) < probability_of_random_jump) {
      do_random_jump = true;
    }
    if (random_integer_in_range(1, 1000000) < probability_of_random_retrig) {
      sf->do_retrig_pitch_changes = (random_integer_in_range(1, 10) < 5);
      go_retrigger_2key(random_integer_in_range(0, 15),
                        random_integer_in_range(0, 15));
    }

    adc_select_input(2);

    // check if a single button is held
    // for purposes of changing the fx params
    int8_t single_key = -1;
    for (uint8_t i = 4; i < 20; i++) {
      if (key_on_buttons[i] > 0) {
        if (single_key == -1) {
          single_key = i;
        } else {
          single_key = -1;
          break;
        }
      }
    }
    if (single_key > -1) {
      DebounceDigits_clear(debouncer_digits);
    }

    if (debounce_beat_repeat > 0) {
      debounce_beat_repeat--;
      if (debounce_beat_repeat == 10) {
        BeatRepeat_repeat(beatrepeat,
                          sf->fx_param[FX_BEATREPEAT][0] * 19000 / 255 + 100);
      } else if (debounce_beat_repeat == 100) {
        BeatRepeat_repeat(beatrepeat, 20);
      }
    }

    int adc;

#ifdef INCLUDE_KNOBS
    // knob X
    adc = FilterExp_update(adcs[0], adc_read());
    if (abs(adc_last[0] - adc) > adc_threshold) {
      adc_debounce[0] = adc_debounce_max;
    }
    if (adc_debounce[0]) {
      adc_last[0] = adc;
      adc_debounce[0]--;
      if (mode_buttons16 == MODE_MASH && single_key > -1) {
        sf->fx_param[single_key - 4][0] = adc * 255 / 4096;
        clear_debouncers();
        DebounceUint8_set(debouncer_uint8[DEBOUNCE_UINT8_LED_BAR],
                          sf->fx_param[single_key - 4][0], 100);
        printf("fx_param %d: %d %d\n", 0, single_key - 4, adc * 255 / 4096);
        if (key_on_buttons[FX_BEATREPEAT + 4] && do_update_beat_repeat == 0) {
          debounce_beat_repeat = 30;
        } else if (key_on_buttons[FX_TIGHTEN + 4]) {
          printf("updating gate\n");
          Gate_set_amount(audio_gate, sf->fx_param[FX_TIGHTEN][0]);
        } else if (key_on_buttons[FX_TREMELO + 4]) {
          lfo_tremelo_step =
              Q16_16_2PI / (12 + (255 - sf->fx_param[single_key - 4][0]) * 2);
        } else if (key_on_buttons[FX_PAN + 4]) {
          lfo_pan_step =
              Q16_16_2PI / (12 + (255 - sf->fx_param[single_key - 4][0]) * 2);
        } else if (key_on_buttons[FX_SCRATCH + 4]) {
          scratch_lfo_hz = sf->fx_param[FX_SCRATCH][0] / 255.0 * 4.0 + 0.1;
          scratch_lfo_inc = round(SCRATCH_LFO_1_HZ * scratch_lfo_hz);
        } else if (key_on_buttons[FX_EXPAND + 4]) {
          update_reverb();
        }
      } else if (adc_startup == 0) {
        if (button_is_pressed(KEY_A)) {
#ifdef INCLUDE_MIDI
          // send out midi cc
          MidiOut_cc(midiout[0], 3, adc * 127 / 4096);
#endif
          sf->bpm_tempo = round(linlin(
              adc, 0, 4095,
              (banks[sel_bank_cur]->sample[sel_sample_cur].snd[FILEZERO]->bpm /
               2),
              (banks[sel_bank_cur]->sample[sel_sample_cur].snd[FILEZERO]->bpm *
               2)));
          sf->bpm_tempo = util_clamp(sf->bpm_tempo, 30, 300);
          clear_debouncers();
          DebounceUint8_set(debouncer_uint8[DEBOUNCE_UINT8_LED_DIAGONAL],
                            adc * 255 / 4096, 100);
          DebounceDigits_set(debouncer_digits, sf->bpm_tempo, 300);
        } else if (button_is_pressed(KEY_B)) {
#ifdef INCLUDE_MIDI
          // send out midi cc
          MidiOut_cc(midiout[0], 6, adc * 127 / 4096);
#endif

        } else if (button_is_pressed(KEY_C)) {
#ifdef INCLUDE_MIDI
          // send out midi cc
          MidiOut_cc(midiout[0], 9, adc * 127 / 4096);
#endif
          // change bank
          uint8_t bank_next_possible =
              banks_with_samples[adc * banks_with_samples_num / 4096];
          uint8_t sample_next_possible =
              sel_sample_cur % banks[bank_next_possible]->num_samples;
          if (bank_next_possible != sel_bank_cur &&
              banks[bank_next_possible]->num_samples > 0 &&
              !banks[bank_next_possible]
                   ->sample[sample_next_possible]
                   .snd[FILEZERO]
                   ->one_shot) {
            sel_bank_next = bank_next_possible;
            sel_sample_next = sample_next_possible;
            fil_current_change = true;
          }
        } else if (button_is_pressed(KEY_D)) {
#ifdef INCLUDE_MIDI
          // send out midi cc
          MidiOut_cc(midiout[0], 12, adc * 127 / 4096);
#endif
          probability_of_random_jump = adc;
          clear_debouncers();
          DebounceUint8_set(debouncer_uint8[DEBOUNCE_UINT8_LED_RANDOM1],
                            adc * 255 / 4096, 100);
        } else {
#ifdef INCLUDE_MIDI
          // send out midi cc
          MidiOut_cc(midiout[0], 0, adc * 127 / 4096);
#endif
        }
      }
    }
#endif

#ifdef BTN_COL_START
    // button handler
    button_handler(bm);
#endif

#ifdef INCLUDE_KNOBS
    // knob Y
    adc_select_input(1);
    adc = FilterExp_update(adcs[1], adc_read());
    if (abs(adc_last[1] - adc) > adc_threshold) {
      adc_debounce[1] = adc_debounce_max;
    }
    if (adc_debounce[1] > 0) {
      adc_last[1] = adc;
      adc_debounce[1]--;
      if (mode_buttons16 == MODE_MASH && single_key > -1) {
        sf->fx_param[single_key - 4][1] = adc * 255 / 4096;
        printf("fx_param %d: %d %d\n", 1, single_key - 4, adc * 255 / 4096);
        if (key_on_buttons[FX_EXPAND + 4]) {
          update_reverb();
        }
      } else if (adc_startup == 0) {
        if (button_is_pressed(KEY_A)) {
#ifdef INCLUDE_MIDI
          // send out midi cc
          MidiOut_cc(midiout[0], 4, adc * 127 / 4096);
#endif

          int16_t adc_original = adc;
          if (adc < 2048 - 200) {
            sf->pitch_val_index = adc * PITCH_VAL_MID / (2048 - 200);
          } else if (adc > 2048 + 200) {
            adc -= 2048 + 200;
            sf->pitch_val_index =
                adc * (PITCH_VAL_MAX - PITCH_VAL_MID) / (2048 - 200) +
                PITCH_VAL_MID;
          } else {
            sf->pitch_val_index = PITCH_VAL_MID;
          }
          clear_debouncers();
          DebounceUint8_set(debouncer_uint8[DEBOUNCE_UINT8_LED_TRIANGLE],
                            adc_original * 255 / 4096, 250);
        } else if (button_is_pressed(KEY_B)) {
#ifdef INCLUDE_MIDI
          // send out midi cc
          MidiOut_cc(midiout[0], 7, adc * 127 / 4096);
#endif
          for (uint8_t channel = 0; channel < 2; channel++) {
            if (adc < 3500) {
              global_filter_index = adc * (resonantfilter_fc_max) / 3500;
              ResonantFilter_setFilterType(resFilter[channel], 0);
              ResonantFilter_setFc(resFilter[channel], global_filter_index);
            } else {
              global_filter_index = resonantfilter_fc_max;
              ResonantFilter_setFilterType(resFilter[channel], 0);
              ResonantFilter_setFc(resFilter[channel], resonantfilter_fc_max);
            }
          }
          clear_debouncers();
          DebounceUint8_set(debouncer_uint8[DEBOUNCE_UINT8_LED_SPIRAL1],
                            adc * 255 / 4096, 200);
        } else if (button_is_pressed(KEY_C)) {
#ifdef INCLUDE_MIDI
          // send out midi cc
          MidiOut_cc(midiout[0], 10, adc * 127 / 4096);
#endif

          // change sample

          // change bank
          uint8_t bank_next_possible = sel_bank_cur;
          uint8_t sample_next_possible =
              adc * banks[sel_bank_cur]->num_samples / 4096;
          if (sample_next_possible != sel_sample_cur &&
              banks[bank_next_possible]->num_samples > 0 &&
              !banks[bank_next_possible]
                   ->sample[sample_next_possible]
                   .snd[FILEZERO]
                   ->one_shot) {
            sel_bank_next = bank_next_possible;
            sel_sample_next = sample_next_possible;
            fil_current_change = true;
          }
        } else if (button_is_pressed(KEY_D)) {
#ifdef INCLUDE_MIDI
          // send out midi cc
          MidiOut_cc(midiout[0], 13, adc * 127 / 4096);
#endif
          probability_of_random_retrig = adc;
          clear_debouncers();
          DebounceUint8_set(debouncer_uint8[DEBOUNCE_UINT8_LED_RANDOM2],
                            adc * 255 / 4096, 100);
        } else {
#ifdef INCLUDE_MIDI
          // send out midi cc
          MidiOut_cc(midiout[0], 1, adc * 127 / 4096);
#endif
        }
      }
    }
#endif

#ifdef INCLUDE_CLOCKINPUT
    // clock input handler
    ClockInput_update(clockinput);
#endif

#ifdef BTN_COL_START
    // button handler
    button_handler(bm);
#endif

#ifdef INCLUDE_KNOBS
    // knob Z
    adc_select_input(0);
    adc = FilterExp_update(adcs[2], adc_read());
    if (abs(adc_last[2] - adc) > adc_threshold) {
      adc_debounce[2] = adc_debounce_max;
    }
    if (adc_debounce[2] > 0) {
      adc_last[2] = adc;
      adc_debounce[2]--;
      if (mode_buttons16 == MODE_MASH && single_key > -1) {
        sf->fx_param[single_key - 4][2] = adc * 255 / 4096;
        printf("fx_param %d: %d %d\n", 2, single_key - 4, adc * 255 / 4096);
      } else if (adc_startup == 0) {
        if (button_is_pressed(KEY_A)) {
#ifdef INCLUDE_MIDI
          // send out midi cc
          MidiOut_cc(midiout[0], 5, adc * 127 / 4096);
#endif
          new_vol = adc * VOLUME_STEPS / 4096;
          // new_vol = 100;
          if (new_vol != sf->vol) {
            sf->vol = new_vol;
            printf("sf-vol: %d\n", sf->vol);
          }
          clear_debouncers();
          DebounceUint8_set(debouncer_uint8[DEBOUNCE_UINT8_LED_WALL],
                            adc * 255 / 4096, 200);
        } else if (button_is_pressed(KEY_B)) {
#ifdef INCLUDE_MIDI
          // send out midi cc
          MidiOut_cc(midiout[0], 8, adc * 127 / 4096);
#endif
          // set the bass volume
          DebounceUint8_set(debouncer_uint8[DEBOUNCE_UINT8_LED_BAR],
                            adc * 255 / 4096, 200);
          WaveBass_set_volume(wavebass, adc);
        } else if (button_is_pressed(KEY_C)) {
#ifdef INCLUDE_MIDI
          // send out midi cc
          MidiOut_cc(midiout[0], 11, adc * 127 / 4096);
#endif

          const uint8_t quantizations[10] = {1,  6,  12,  24,  48,
                                             64, 96, 144, 192, 192};
          printf("quantization: %d\n", quantizations[adc * 9 / 4096]);
          Sequencer_quantize(
              sf->sequencers[mode_buttons16][sf->sequence_sel[mode_buttons16]],
              quantizations[adc * 9 / 4096]);
          DebounceUint8_set(debouncer_uint8[DEBOUNCE_UINT8_LED_WALL],
                            adc * 255 / 4096, 200);
        } else if (button_is_pressed(KEY_D)) {
#ifdef INCLUDE_MIDI
          // send out midi cc
          MidiOut_cc(midiout[0], 14, adc * 127 / 4096);
#endif
          probability_of_random_tunnel = adc * 1000 / 4096;
          if (probability_of_random_tunnel < 100) {
            probability_of_random_tunnel = 0;
          }
          clear_debouncers();
          DebounceUint8_set(debouncer_uint8[DEBOUNCE_UINT8_LED_RANDOM2],
                            adc * 255 / 4096, 100);
        } else {
#ifdef INCLUDE_MIDI
          // send out midi cc
          MidiOut_cc(midiout[0], 2, adc * 127 / 4096);
#endif
        }
      }
    }
#endif

    // TODO: dead code?
    // update the text if any
    LEDText_update(ledtext, leds);
    // TODO: redundant code?
    LEDS_render(leds);

#ifdef BTN_COL_START
    // button handler
    button_handler(bm);
#endif

#ifdef INCLUDE_KEYBOARD
    // check keyboard
    run_keyboard();
#endif

    // load the new sample if variation changed
    if (sel_variation_next != sel_variation) {
      if (!audio_callback_in_mute) {
        while (!sync_using_sdcard) {
          sleep_us(250);
        }
        while (sync_using_sdcard) {
          sleep_us(250);
        }
      }
      sync_using_sdcard = true;
      // measure the time it takes
      uint32_t time_start = time_us_32();
      FRESULT fr = f_close(&fil_current);
      if (fr != FR_OK) {
        debugf("[zeptocore] f_close error: %s\n", FRESULT_str(fr));
      }
      sprintf(fil_current_name, "bank%d/%d.%d.wav", sel_bank_cur,
              sel_sample_cur, sel_variation_next);
      fr = f_open(&fil_current, fil_current_name, FA_READ);
      if (fr != FR_OK) {
        debugf("[zeptocore] f_close error: %s\n", FRESULT_str(fr));
      }

      // TODO: fix this
      // if sel_variation_next == 0
      phases[0] = round(
          ((float)phases[0] * (float)sel_variation_scale[sel_variation_next]) /
          (float)sel_variation_scale[sel_variation]);

      sel_variation = sel_variation_next;
      sync_using_sdcard = false;
      printf("[zeptocore] loading new sample variation took %d us\n",
             time_us_32() - time_start);
    }
  }
}
