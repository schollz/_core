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
//
#include "midicallback.h"
#include "onewiremidi.h"
#ifdef INCLUDE_MIDI
#include "midi_comm_callback.h"
#endif
#ifdef INCLUDE_SSD1306
#include "image.h"
#include "ssd1306.h"
#endif
#include "break_knob.h"

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

void make_random_sequence(uint8_t adcValue) {
  if (adcValue > 255) adcValue = 255;
  if (adcValue < 32) {
    // normal
    do_retrig_at_end_of_phrase = false;
    random_sequence_length = 0;
  } else if (adcValue < 255 - 32) {
    do_retrig_at_end_of_phrase = false;
    uint8_t sequence_lengths[11] = {
        1, 2, 4, 6, 8, 12, 16, 24, 32, 48, 64,
    };
    random_sequence_length =
        sequence_lengths[((int16_t)(adcValue - 32) * 11 / (255 - 32)) % 11];
  } else {
    // new random sequence
    regenerate_random_sequence_arr();
    random_sequence_length = 8;
    do_retrig_at_end_of_phrase = true;
  }
  clear_debouncers();
  DebounceUint8_set(debouncer_uint8[DEBOUNCE_UINT8_LED_TRIANGLE], adcValue,
                    200);
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

  uint8_t new_vol;
  //   (
  // a=Array.fill(72,{ arg i;
  //   (i+60).midicps.round.asInteger
  // });
  // a.postln;
  // )
  ClockInput *clockinput = NULL;
  Onewiremidi *onewiremidi = NULL;
  if (use_onewiremidi) {
    // setup one wire midi
    onewiremidi =
        Onewiremidi_new(pio0, 3, CLOCK_INPUT_GPIO, midi_note_on, midi_note_off,
                        midi_start, midi_continue, midi_stop, midi_timing);
  } else {
    clockinput = ClockInput_create(CLOCK_INPUT_GPIO, clock_handling_up,
                                   clock_handling_down, clock_handling_start);
  }

  FilterExp *adcs[3];
  int adc_last[3] = {0, 0, 0};
  int adc_debounce[3] = {0, 0, 0};
  const int adc_threshold_const = 200;
  int adc_threshold = adc_threshold_const;
  const int adc_debounce_max = 25;
  uint16_t adc_startup = 300;
  // TODO add debounce for the adc detection
  for (uint8_t i = 0; i < 3; i++) {
    adcs[i] = FilterExp_create(10);
  }

  uint8_t debounce_beat_repeat = 0;
  uint16_t debounce_sel_variation_next = 0;
  uint8_t sample_selection_index_last = 0;
  uint8_t debounce_sample_selection = 0;
  uint8_t sample_selection_index = 0;

  // debug test
  printStringWithDelay("zv6.2.13");

  // print to screen
  printf("version=v6.2.13\n");

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

#ifdef INCLUDE_SSD1306
  ssd1306_t disp;
  disp.external_vcc = false;
  ssd1306_init(&disp, 128, 64, 0x3C, i2c_default);
  ssd1306_clear(&disp);
#endif

#ifdef DETROITUNDERGROUND
  probability_of_random_jump = 25;
  probability_of_random_retrig = 400;
  probability_of_random_tunnel = 60;
#endif

  regenerate_random_sequence_arr();

  KnobChange *knob_change_arcade[8];
  ADS7830 *arcade_ads7830 = NULL;
  if (is_arcade_box) {
    arcade_ads7830 = ADS7830_malloc(ADS7830_ADDR);
    // create array of knob changes
    for (uint8_t i = 0; i < 8; i++) {
      knob_change_arcade[i] = KnobChange_malloc(2);
    }
  }

  bool sel_sample_knob_ready = false;
  clock_start_stop_sync = true;
  while (1) {
#ifdef INCLUDE_MIDI
    tud_task();
    midi_comm_task(midi_comm_callback_fn, midi_note_on, midi_note_off,
                   midi_start, midi_continue, midi_stop, midi_timing);
#endif

    if (do_switch_between_clock_and_midi) {
      do_switch_between_clock_and_midi = false;
      if (use_onewiremidi) {
        // TODO: switching back doesn't work yet
        // // switch to clock
        // Onewiremidi_destroy(onewiremidi);
        // clockinput =
        //     ClockInput_create(CLOCK_INPUT_GPIO, clock_handling_up,
        //                       clock_handling_down, clock_handling_start);
      } else {
        // switch to one wire midi
        ClockInput_destroy(clockinput);
        onewiremidi = Onewiremidi_new(pio0, 3, CLOCK_INPUT_GPIO, midi_note_on,
                                      midi_note_off, midi_start, midi_continue,
                                      midi_stop, midi_timing);
      }
      use_onewiremidi = !use_onewiremidi;
    }

    // if in startup deduct
    if (adc_startup > 0) {
      adc_startup--;
      if (adc_startup == 0) {
        printf("adc startup done\n");
      }
      // if (adc_startup == 0) {
      //   for (int i = 1; i < 2; i++) {
      //     PIO p = (i == 0) ? pio0 : pio1;
      //     printf("PIO%d:\n", i);
      //     for (int sm = 2; sm < 4; sm++) {
      //       if (pio_sm_is_claimed(p, sm)) {
      //         printf("  State Machine %d: USED\n", sm);
      //       } else {
      //         printf("  State Machine %d: NOT USED\n", sm);
      //       }
      //     }
      //     sleep_ms(1);
      //   }
      // }
    }

    // check for input
    int char_input = getchar_timeout_us(10);
    if (char_input >= 0) {
      if (char_input == 118) {
        printf("version=v6.2.13\n");
      }
    }

#ifdef BTN_COL_START
    // button handler
    if (button_handler(bm)) {
      // reset knob debouncers
      adc_threshold = 10000;
      for (uint8_t i = 0; i < 3; i++) {
        adc_debounce[i] = 0;
      }
    } else {
      adc_threshold = adc_threshold_const;
    }
#endif

#ifdef INCLUDE_CLOCKINPUT
    if (!use_onewiremidi) {
      // clock input handler
      ClockInput_update(clockinput);
      if (clock_in_do) {
        clock_input_absent_zeptocore =
            ClockInput_timeSinceLast(clockinput) > 1000000;
      }
      if (!clock_start_stop_sync && clock_in_do) {
        if ((time_us_32() - clock_in_last_time) > 2 * clock_in_diff_2x) {
          clock_in_ready = false;
          clock_in_do = false;
          clock_in_activator = 0;
        }
      }
    } else {
      Onewiremidi_receive(onewiremidi);
    }
#endif

    if (MessageSync_hasMessage(messagesync)) {
      MessageSync_print(messagesync);
      MessageSync_clear(messagesync);
    }

#ifdef INCLUDE_SSD1306
    char buf_ssd1306[16];
    sprintf(buf_ssd1306, "%d", beat_current);
    ssd1306_clear(&disp);
    ssd1306_draw_string(&disp, 8, 24, 1, buf_ssd1306);
    ssd1306_bmp_show_image_with_offset(&disp, output_bmp_data, output_bmp_size,
                                       0, 64 - 17);
    // tile 4x4 on the right 64x64 pixels of a 14x14 image
    for (int i = 0; i < 4; i++) {
      for (int j = 0; j < 4; j++) {
        ssd1306_bmp_show_image_with_offset(&disp, sun_solid2_bmp_data,
                                           sun_solid2_bmp_size, 64 + i * 16,
                                           64 - 17 - j * 16 + 1);
      }
    }
    ssd1306_show(&disp);
#endif

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
#else
#ifdef PRINT_AUDIOBLOCKDROPS
    // random stuff
    if (random_integer_in_range(1, 10000) < 80) {
      // printf("random retrig\n");
      key_do_jump(random_integer_in_range(0, 15));
    } else if (random_integer_in_range(1, 10000) < 5) {
      // printf("random retrigger\n");
      go_retrigger_2key(random_integer_in_range(0, 15),
                        random_integer_in_range(0, 15));
    }
#endif
#endif

    if (random_integer_in_range(1, 1000000) < probability_of_random_retrig) {
      sf->do_retrig_pitch_changes = (random_integer_in_range(1, 10) < 5);
      go_retrigger_2key(random_integer_in_range(0, 15),
                        random_integer_in_range(0, 15));
    }

    adc_select_input(2);

    // check if a single button is held
    // for purposes of changing the fx params
    int8_t single_key = -1;
    if (key_on_buttons[0] > 0 && key_on_buttons[1] > 0) {
    } else {
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
    uint16_t adc_raw = adc_read();
    adc = FilterExp_update(adcs[0], adc_raw);
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
        } else if (key_on_buttons[FX_DELAY + 4]) {
          Delay_setFeedbackf(delay, (float)adc / 8192.0f + 0.49f);
        } else if (key_on_buttons[FX_TIGHTEN + 4]) {
          printf("updating gate\n");
          Gate_set_amount(audio_gate, sf->fx_param[FX_TIGHTEN][0]);
          // deactivated
          // } else if (key_on_buttons[FX_TREMELO + 4]) {
          //   lfo_tremelo_step =
          //       Q16_16_2PI / (12 + (255 - sf->fx_param[single_key - 4][0]) *
          //       2);
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
          uint16_t bpm_new_tempo =
              banks[sel_bank_cur]->sample[sel_sample_cur].snd[FILEZERO]->bpm;
          bpm_new_tempo = round(
              linlin(adc, 0, 4095, bpm_new_tempo / 2, bpm_new_tempo * 3 / 2));
          if (bpm_new_tempo % 10 == 1 || bpm_new_tempo % 10 == 9) {
            // round to nearest 5
            bpm_new_tempo = (bpm_new_tempo / 5) * 5;
          } else if (bpm_new_tempo % 10 == 3 || bpm_new_tempo % 10 == 7) {
            // round to nearest 2
            bpm_new_tempo = (bpm_new_tempo / 2) * 2;
          }
          printf("bpm_new_tempo: %d\n", bpm_new_tempo);
          sf->bpm_tempo = util_clamp(bpm_new_tempo, 30, 300);
          clear_debouncers();
          DebounceUint8_set(debouncer_uint8[DEBOUNCE_UINT8_LED_DIAGONAL],
                            adc * 255 / 4096, 100);
          DebounceDigits_set(debouncer_digits, sf->bpm_tempo, 300);
        } else if (button_is_pressed(KEY_B)) {
#ifdef INCLUDE_MIDI
          // send out midi cc
          MidiOut_cc(midiout[0], 6, adc * 127 / 4096);
#endif
          make_random_sequence(adc * 255 / 4096);
        } else if (button_is_pressed(KEY_C)) {
          // C + X
#ifdef INCLUDE_MIDI
          // send out midi cc
          MidiOut_cc(midiout[0], 9, adc * 127 / 4096);
#endif
          sample_selection_index = adc_raw * sample_selection_num / 4096;
          printf("sample_selection_index: %d\n", sample_selection_index);
        } else if (button_is_pressed(KEY_D)) {
#ifdef INCLUDE_MIDI
          // send out midi cc
          MidiOut_cc(midiout[0], 12, adc * 127 / 4096);
#endif
          probability_of_random_jump = adc * 100 / 4096;
          clear_debouncers();
          DebounceUint8_set(debouncer_uint8[DEBOUNCE_UINT8_LED_RANDOM1],
                            adc * 255 / 4096, 100);
        } else {
          if (global_knobx_sample_selector) {
            sample_selection_index = adc_raw * sample_selection_num / 4096;
          }
#ifdef INCLUDE_MIDI
          // send out midi cc
          MidiOut_cc(midiout[0], 0, adc * 127 / 4096);
#endif
        }
      }
    }
#endif

    if (sample_selection_index_last != sample_selection_index) {
      printf("sample_selection_index: %d\n", sample_selection_index);
      sample_selection_index_last = sample_selection_index;
      debounce_sample_selection = 4;
    } else if (debounce_sample_selection > 0) {
      debounce_sample_selection--;
      if (debounce_sample_selection == 0) {
        uint8_t f_sel_bank_next = sample_selection[sample_selection_index].bank;
        uint8_t f_sel_sample_next =
            sample_selection[sample_selection_index].sample;
        if (f_sel_bank_next != sel_bank_cur ||
            f_sel_sample_next != sel_sample_cur) {
          sel_bank_next = f_sel_bank_next;
          sel_sample_next = f_sel_sample_next;
          printf("[zeptocore] %d bank %d, sample %d\n", sample_selection_index,
                 sel_bank_next, sel_sample_next);
          fil_current_change = true;
        }
      }
    }

#ifdef BTN_COL_START
    if (!is_arcade_box) button_handler(bm);
#endif

#ifdef INCLUDE_CLOCKINPUT
    if (!use_onewiremidi) {
      // clock input handler
      ClockInput_update(clockinput);
      if (clock_in_do) {
        clock_input_absent_zeptocore =
            ClockInput_timeSinceLast(clockinput) > 1000000;
      }
      if (!clock_start_stop_sync && clock_in_do) {
        if ((time_us_32() - clock_in_last_time) > 2 * clock_in_diff_2x) {
          clock_in_ready = false;
          clock_in_do = false;
          clock_in_activator = 0;
        }
      }
    } else {
      Onewiremidi_receive(onewiremidi);
    }
#endif

    break_fx_update();

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
        } else if (key_on_buttons[FX_DELAY + 4]) {
          Delay_setDuration(
              delay, powf(2, linlin((float)adc, 0.0f, 4095.0f, 6.64f, 13.28f)));
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
#define FILTER_ZERO_SPACING 500
            if (adc < 2048 - FILTER_ZERO_SPACING) {
              global_filter_index =
                  adc * (resonantfilter_fc_max) / (2048 - FILTER_ZERO_SPACING);
              global_filter_lphp = 0;
              ResonantFilter_setFilterType(resFilter[channel],
                                           global_filter_lphp);
              ResonantFilter_setFc(resFilter[channel], global_filter_index);
            } else if (adc > 2048 + FILTER_ZERO_SPACING) {
              global_filter_index = (adc - (2048 + FILTER_ZERO_SPACING)) *
                                    (resonantfilter_fc_max) /
                                    (2048 - FILTER_ZERO_SPACING);
              global_filter_lphp = 1;
              ResonantFilter_setFilterType(resFilter[channel],
                                           global_filter_lphp);
              ResonantFilter_setFc(resFilter[channel], global_filter_index);
            } else {
              global_filter_index = resonantfilter_fc_max;
              global_filter_lphp = 0;
              ResonantFilter_setFilterType(resFilter[channel],
                                           global_filter_lphp);
              ResonantFilter_setFc(resFilter[channel], resonantfilter_fc_max);
            }
          }
          clear_debouncers();
          DebounceUint8_set(debouncer_uint8[DEBOUNCE_UINT8_LED_SPIRAL1],
                            adc * 255 / 4096, 200);
        } else if (button_is_pressed(KEY_C)) {
          // C + Y
#ifdef INCLUDE_MIDI
          // send out midi cc
          MidiOut_cc(midiout[0], 10, adc * 127 / 4096);
#endif
          probability_of_random_tunnel = adc * 1000 / 4096;
          if (probability_of_random_tunnel < 100) {
            probability_of_random_tunnel = 0;
          }
          clear_debouncers();
          DebounceUint8_set(debouncer_uint8[DEBOUNCE_UINT8_LED_RANDOM2],
                            adc * 255 / 4096, 100);
        } else if (button_is_pressed(KEY_D)) {
          // D + Y
#ifdef INCLUDE_MIDI
          // send out midi cc
          MidiOut_cc(midiout[0], 13, adc * 127 / 4096);
#endif
          break_knob_set_point = adc * 1024 / 4096;
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
    if (!use_onewiremidi) {
      // clock input handler
      ClockInput_update(clockinput);
      if (clock_in_do) {
        clock_input_absent_zeptocore =
            ClockInput_timeSinceLast(clockinput) > 1000000;
      }
      if (!clock_start_stop_sync && clock_in_do) {
        if ((time_us_32() - clock_in_last_time) > 2 * clock_in_diff_2x) {
          clock_in_ready = false;
          clock_in_do = false;
          clock_in_activator = 0;
        }
      }
    } else {
      Onewiremidi_receive(onewiremidi);
    }
#endif

#ifdef BTN_COL_START
    if (!is_arcade_box) button_handler(bm);
#endif

#ifdef INCLUDE_CLOCKINPUT
    if (!use_onewiremidi) {
      // clock input handler
      ClockInput_update(clockinput);
      if (clock_in_do) {
        clock_input_absent_zeptocore =
            ClockInput_timeSinceLast(clockinput) > 1000000;
      }
      if (!clock_start_stop_sync && clock_in_do) {
        if ((time_us_32() - clock_in_last_time) > 2 * clock_in_diff_2x) {
          clock_in_ready = false;
          clock_in_do = false;
          clock_in_activator = 0;
        }
      }
    } else {
      Onewiremidi_receive(onewiremidi);
    }
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
#ifdef INCLUDE_SINEBASS
          WaveBass_set_volume(wavebass, adc);
#endif
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
          clear_debouncers();
          DebounceUint8_set(debouncer_uint8[DEBOUNCE_UINT8_LED_WALL],
                            adc * 255 / 4096, 200);
        } else if (button_is_pressed(KEY_D)) {
          // D + Z
#ifdef INCLUDE_MIDI
          // send out midi cc
          MidiOut_cc(midiout[0], 14, adc * 127 / 4096);
#endif
          // change the grimoire rune
          grimoire_rune = adc * 7 / 4096;
          clear_debouncers();
          DebounceUint8_set(debouncer_uint8[DEBOUNCE_UINT8_LED_GRIMOIRE],
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

    if (is_arcade_box) {
      // volume                   tempo
      // sample selection         dj filter
      // grimoire selection       grimoire probability
      // random sequencer         random jump
      // read the arcade box knobs
      for (uint8_t i = 0; i < 8; i++) {
        int16_t adcValue = KnobChange_update(
            knob_change_arcade[i], (int16_t)ADS7830_read(arcade_ads7830, i));
        if (adcValue > -1) {
// printf("knob %d: %d\n", i, adcValue);
#ifdef INCLUDE_MIDI
          MidiOut_cc(midiout[0], i + 60, adcValue * 127 / 255);
#endif
          if (i == 0) {
            // change volume
            new_vol = (255 - adcValue) * VOLUME_STEPS * 6 / 7 / 255;
            if (new_vol != sf->vol) {
              sf->vol = new_vol;
              printf("sf-vol: %d\n", sf->vol);
            }
            clear_debouncers();
            DebounceUint8_set(debouncer_uint8[DEBOUNCE_UINT8_LED_BAR],
                              255 - adcValue, 200);
          } else if (i == 1) {
            // change bpm
            if (adcValue < 16) {
              sf->bpm_tempo = banks[sel_bank_cur]
                                  ->sample[sel_sample_cur]
                                  .snd[FILEZERO]
                                  ->bpm;
            } else {
              sf->bpm_tempo = util_clamp(
                  (((adcValue - 16) * (240 - 60) / (255 - 16)) / 2) * 2 + 60,
                  60, 240);
            }
          } else if (i == 2) {
            // <change_sample>
            sample_selection_index =
                adcValue * (sample_selection_num - 1) / 255;
            uint8_t f_sel_bank_next =
                sample_selection[sample_selection_index].bank;
            uint8_t f_sel_sample_next =
                sample_selection[sample_selection_index].sample;
            if (f_sel_bank_next != sel_bank_cur ||
                f_sel_sample_next != sel_sample_cur) {
              sel_bank_next = f_sel_bank_next;
              sel_sample_next = f_sel_sample_next;
              printf("[zeptocore] %d bank %d, sample %d\n",
                     sample_selection_index, sel_bank_next, sel_sample_next);
              fil_current_change = true;
            }
            clear_debouncers();
            DebounceUint8_set(debouncer_uint8[DEBOUNCE_UINT8_LED_BAR], adcValue,
                              200);
            // </change_sample>
          } else if (i == 3) {
            // <dj_style_filter>
            for (uint8_t channel = 0; channel < 2; channel++) {
              if (adcValue < 128) {
                if (adcValue < 5) {
                  global_filter_index = resonantfilter_fc_max;
                } else {
                  global_filter_index =
                      (128 - adcValue) * (resonantfilter_fc_max) / 128;
                }
                global_filter_lphp = 0;
                ResonantFilter_setFilterType(resFilter[channel],
                                             global_filter_lphp);
                ResonantFilter_setFc(resFilter[channel], global_filter_index);
              } else if (adcValue >= 128) {
                global_filter_index =
                    (128 - (adcValue - 128)) * resonantfilter_fc_max / 128;
                global_filter_lphp = 1;
                ResonantFilter_setFilterType(resFilter[channel],
                                             global_filter_lphp);
                ResonantFilter_setFc(resFilter[channel], global_filter_index);
              } else {
                global_filter_index = resonantfilter_fc_max;
                global_filter_lphp = 0;
                ResonantFilter_setFilterType(resFilter[channel],
                                             global_filter_lphp);
                ResonantFilter_setFc(resFilter[channel], resonantfilter_fc_max);
              }
            }
            clear_debouncers();
            DebounceUint8_set(debouncer_uint8[DEBOUNCE_UINT8_LED_SPIRAL1],
                              adcValue, 200);
            // </dj_style_filter>
          } else if (i == 4) {
            // <grimoire_selection>
            // change the grimoire rune
            grimoire_rune = adcValue * 7 / 255;
            clear_debouncers();
            DebounceUint8_set(debouncer_uint8[DEBOUNCE_UINT8_LED_GRIMOIRE],
                              adcValue, 100);
            // </grimoire_selection>
          } else if (i == 5) {
            // <grimoire_probability>
            break_knob_set_point = adcValue * 1024 / 255;
            clear_debouncers();
            DebounceUint8_set(debouncer_uint8[DEBOUNCE_UINT8_LED_RANDOM2],
                              adcValue, 100);
            // </grimoire_probability>
          } else if (i == 6) {
            // <random_sequencer>
            make_random_sequence(adcValue);
            // </random_sequencer>
          } else if (i == 7) {
            // <random_jump>
            if (adcValue < 128) {
              sf->stay_in_sync = true;
              probability_of_random_jump = adcValue * 100 / 128;
            } else if (adcValue >= 128) {
              sf->stay_in_sync = false;
              probability_of_random_jump = (255 - adcValue) * 100 / 128;
            }
            clear_debouncers();
            DebounceUint8_set(debouncer_uint8[DEBOUNCE_UINT8_LED_RANDOM1],
                              adcValue, 100);
            // </random_jump>
          }
        }
      }
    }

    // TODO: dead code?
    // update the text if any
    LEDText_update(ledtext, leds);
    // TODO: redundant code?
    LEDS_render(leds);

#ifdef BTN_COL_START
    if (!is_arcade_box) button_handler(bm);
#endif

#ifdef INCLUDE_CLOCKINPUT
    if (!use_onewiremidi) {
      // clock input handler
      ClockInput_update(clockinput);
      if (clock_in_do) {
        clock_input_absent_zeptocore =
            ClockInput_timeSinceLast(clockinput) > 1000000;
      }
      if (!clock_start_stop_sync && clock_in_do) {
        if ((time_us_32() - clock_in_last_time) > 2 * clock_in_diff_2x) {
          clock_in_ready = false;
          clock_in_do = false;
          clock_in_activator = 0;
        }
      }
    } else {
      Onewiremidi_receive(onewiremidi);
    }
#endif

#ifdef INCLUDE_CLOCKINPUT
    if (!use_onewiremidi) {
      // clock input handler
      ClockInput_update(clockinput);
      if (clock_in_do) {
        clock_input_absent_zeptocore =
            ClockInput_timeSinceLast(clockinput) > 1000000;
      }
      if (!clock_start_stop_sync && clock_in_do) {
        if ((time_us_32() - clock_in_last_time) > 2 * clock_in_diff_2x) {
          clock_in_ready = false;
          clock_in_do = false;
          clock_in_activator = 0;
        }
      }
    } else {
      Onewiremidi_receive(onewiremidi);
    }
#endif

#ifdef INCLUDE_KEYBOARD
    // check keyboard
    run_keyboard();
#endif

    // load the new sample if variation changed
    if (debounce_sel_variation_next > 0) {
      debounce_sel_variation_next--;
    } else if (sel_variation_next != sel_variation) {
      debounce_sel_variation_next = 50;
      bool do_try_change = false;
      if (!audio_callback_in_mute) {
        // uint32_t time_start = time_us_32();
        sleep_us(100);
        while (!sync_using_sdcard) {
          sleep_us(100);
        }
        // printf("sync1: %ld\n", time_us_32() - time_start);
        uint32_t time_start = time_us_32();
        sleep_us(100);
        while (sync_using_sdcard) {
          sleep_us(100);
        }
        printf("sync2: %ld\n", time_us_32() - time_start);
        // make sure the audio block was faster than usual
        if (time_us_32() - time_start < 4000) {
          do_try_change = true;
        }
      }
      if (do_try_change) {
        sync_using_sdcard = true;
        // measure the time it takes
        uint32_t time_start = time_us_32();
        FRESULT fr = f_close(&fil_current);
        if (fr != FR_OK) {
          debugf("[zeptocore] f_close error: %s\n", FRESULT_str(fr));
        }
        sprintf(fil_current_name, "bank%d/%d.%d.wav", sel_bank_cur + 1,
                sel_sample_cur, sel_variation_next + audio_variant * 2);
        fr = f_open(&fil_current, fil_current_name, FA_READ);
        if (fr != FR_OK) {
          debugf("[zeptocore] f_close error: %s\n", FRESULT_str(fr));
        }

        // TODO: fix this
        // if sel_variation_next == 0
        phases[0] = round(((float)phases[0] *
                           (float)sel_variation_scale[sel_variation_next]) /
                          (float)sel_variation_scale[sel_variation]);

        sel_variation = sel_variation_next;
        sync_using_sdcard = false;
        printf("[zeptocore] loading new sample variation took %d us\n",
               time_us_32() - time_start);
      }
    }
  }
}
