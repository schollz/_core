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

void clock_handling(int time_diff) {
  sf->bpm_tempo = 60000000 / (time_diff * 2);
  // printf("[zeptocore] clock_handling: %d, bpm: %d\n", time_diff, new_bpm);
  clock_in_debounce = 2000;
  clock_in_ready = true;
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
  ClockInput *clockinput = ClockInput_create(CLOCK_INPUT_GPIO, clock_handling);

  FilterExp *adcs[3];
  int adc_last[3] = {0, 0, 0};
  int adc_debounce[3] = {0, 0, 0};
  const int adc_threshold = 200;
  const int adc_debounce_max = 250;
  // TODO add debounce for the adc detection
  for (uint8_t i = 0; i < 3; i++) {
    adcs[i] = FilterExp_create(10);
  }

  while (1) {
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

    sleep_ms(1);
    int adc;
#ifdef INCLUDE_SINEBASS
    if (sinebass_update_counter < 30) {
      sinebass_update_counter++;
      if (sinebass_update_counter == 10) {
        SinOsc_wave(sinosc[0], sinebass_update_note);
      } else if (sinebass_update_counter == 20) {
        if (sinebass_update_note == 0) {
          SinOsc_wave(sinosc[1], 0);
        } else {
          SinOsc_wave(sinosc[1], sinebass_update_note + 12);
        }
      } else if (sinebass_update_counter == 30) {
        if (sinebass_update_note == 0) {
          SinOsc_wave(sinosc[2], 0);
        } else {
          SinOsc_wave(sinosc[2], sinebass_update_note + 12 + 7);
        }
      }
    }
#endif

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
        DebounceUint8_set(debouncer_uint8[DEBOUNCE_UINT8_LED_BAR],
                          sf->fx_param[single_key - 4][0], 100);
        printf("fx_param %d: %d %d\n", 0, single_key - 4, adc * 255 / 4096);
      } else {
        if (button_is_pressed(KEY_SHIFT)) {
          sf->bpm_tempo = adc * 50 / 4096 * 5 + 50;
        } else if (button_is_pressed(KEY_A)) {
          gate_threshold = adc *
                           (30 * (44100 / SAMPLES_PER_BUFFER) / sf->bpm_tempo) /
                           4096 * 2;
        } else if (button_is_pressed(KEY_B)) {
          pitch_val_index = adc * PITCH_VAL_MAX / 4096;
          if (pitch_val_index >= PITCH_VAL_MAX) {
            pitch_val_index = PITCH_VAL_MAX - 1;
          }
        } else if (button_is_pressed(KEY_C)) {
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
    sleep_ms(1);
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
        if (button_is_pressed(KEY_SHIFT)) {
        } else if (button_is_pressed(KEY_A)) {
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
        } else if (button_is_pressed(KEY_B)) {
        } else if (button_is_pressed(KEY_C)) {
        }
      }
    }
#endif

#ifdef INCLUDE_CLOCKINPUT
    // clock input handler
    ClockInput_update(clockinput);
    if (clock_in_debounce > 0) {
      clock_in_debounce--;
    }
#endif

#ifdef INCLUDE_KNOBS
    // knob Z
    adc_select_input(0);
    sleep_ms(1);
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
        if (button_is_pressed(KEY_SHIFT)) {
          new_vol = adc * VOLUME_STEPS / 4096;
          // new_vol = 100;
          if (new_vol != sf->vol) {
            sf->vol = new_vol;
            printf("sf-vol: %d\n", sf->vol);
          }
        } else if (button_is_pressed(KEY_A)) {
          for (uint8_t channel = 0; channel < 2; channel++) {
            ResonantFilter_setQ(resFilter[channel],
                                adc * (resonantfilter_q_max) / 4096);
          }
        } else if (button_is_pressed(KEY_B)) {
        } else if (button_is_pressed(KEY_C)) {
          // const uint8_t quantizations[7] = {1, 6, 12, 24, 48, 96, 192};
          // printf("quantization: %d\n", quantizations[adc * 7 / 4096]);
          // Chain_quantize_current(chain, quantizations[adc * 7 / 4096]);
        }
      }
    }
#endif

    // update the text if any
    LEDText_update(ledtext, leds);
    LEDS_render(leds);

#ifdef INCLUDE_KEYBOARD
    // check keyboard
    run_keyboard();
#endif
  }
}