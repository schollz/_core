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
          sleep_ms(100);
        } else {
          sleep_ms(400);
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

void clock_handling_up(int time_diff) {
  sf->bpm_tempo = 60000000 / (time_diff * 2);
  clock_in_ready = true;
}

void clock_handling_down(int time_diff) {
  printf("[zeptocore] clock_handling_down: %d\n", time_diff);
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

  printf("entering while loop\n");

  uint pressed2 = 0;
  uint8_t new_vol;
  //   (
  // a=Array.fill(72,{ arg i;
  //   (i+60).midicps.round.asInteger
  // });
  // a.postln;
  // )
  ClockInput *clockinput = ClockInput_create(
      CLOCK_INPUT_GPIO, clock_handling_up, clock_handling_down);

  FilterExp *adcs[3];
  int adc_last[3] = {0, 0, 0};
  int adc_debounce[3] = {0, 0, 0};
  const int adc_threshold = 200;
  const int adc_debounce_max = 250;
  // TODO add debounce for the adc detection
  for (uint8_t i = 0; i < 3; i++) {
    adcs[i] = FilterExp_create(10);
  }

  uint8_t debounce_beat_repeat = 0;

  // debug test
  printStringWithDelay("v0.0.9");

  while (1) {
    // TODO: check timing of this?

    if (MessageSync_hasMessage(messagesync)) {
      MessageSync_print(messagesync);
      MessageSync_clear(messagesync);
    }

#ifdef PRINT_SDCARD_TIMING
    // random stuff
    if (random_integer_in_range(1, 10000) < 10) {
      // printf("random retrig\n");
      key_do_jump(random_integer_in_range(0, 15));
    } else if (random_integer_in_range(1, 10000) < 5) {
      // printf("random retrigger\n");
      go_retrigger_2key(1, 1);
    }
#endif

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
    if (adc_debounce[0] > 0) {
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
        }
      } else {
        if (button_is_pressed(KEY_A)) {
          sf->bpm_tempo = adc * 50 / 4096 * 5 + 50;
          clear_debouncers();
          DebounceUint8_set(debouncer_uint8[DEBOUNCE_UINT8_LED_DIAGONAL],
                            adc * 255 / 4096, 100);
          DebounceDigits_set(debouncer_digits, sf->bpm_tempo, 400);
        } else if (button_is_pressed(KEY_B)) {
          gate_threshold = adc *
                           (30 * (44100 / SAMPLES_PER_BUFFER) / sf->bpm_tempo) /
                           4096 * 2;
        } else if (button_is_pressed(KEY_C)) {
          pitch_val_index = adc * PITCH_VAL_MAX / 4096;
          if (pitch_val_index >= PITCH_VAL_MAX) {
            pitch_val_index = PITCH_VAL_MAX - 1;
          }
        } else if (button_is_pressed(KEY_D)) {
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
      } else {
        if (button_is_pressed(KEY_A)) {
        } else if (button_is_pressed(KEY_B)) {
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
        } else if (button_is_pressed(KEY_D)) {
        }
      }
    }
#endif

#ifdef INCLUDE_CLOCKINPUT
    // clock input handler
    ClockInput_update(clockinput);
    if (clock_in_do) {
      if (ClockInput_time_since(clockinput) > 1000000) {
        beat_current = 0;
      }
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
      } else {
        if (button_is_pressed(KEY_A)) {
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
        } else if (button_is_pressed(KEY_C)) {
          const uint8_t quantizations[10] = {1,  6,  12,  24,  48,
                                             64, 96, 144, 192, 192};
          printf("quantization: %d\n", quantizations[adc * 9 / 4096]);
          Sequencer_quantize(sf->sequencers[mode_buttons16][0],
                             quantizations[adc * 9 / 4096]);
          DebounceUint8_set(debouncer_uint8[DEBOUNCE_UINT8_LED_WALL],
                            adc * 255 / 4096, 200);
        } else if (button_is_pressed(KEY_D)) {
        }
      }
    }
#endif

    // TODO: dead code?
    // update the text if any
    LEDText_update(ledtext, leds);
    // TODO: redundant code?
    LEDS_render(leds);

#ifdef INCLUDE_KEYBOARD
    // check keyboard
    run_keyboard();
#endif
  }
}