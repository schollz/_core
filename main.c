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

#include "lib/includes.h"

static uint8_t dub_step_numerator[] = {1, 1, 1, 1, 1, 1, 1, 1};
static uint8_t dub_step_denominator[] = {2, 3, 4, 8, 8, 12, 12, 16};
static uint8_t dub_step_steps[] = {8, 12, 16, 32, 16, 16};
// timer
bool repeating_timer_callback(struct repeating_timer *t) {
  if (!fil_is_open) {
    return true;
  }
  if (bpm_last != sf->bpm_tempo) {
    printf("updating bpm timer: %d-> %d\n", bpm_last, sf->bpm_tempo);
    bpm_last = sf->bpm_tempo;

    cancel_repeating_timer(&timer);
    add_repeating_timer_us(-(round(30000000 / sf->bpm_tempo / 96)),
                           repeating_timer_callback, NULL, &timer);
  }
  bpm_timer_counter++;
  if (retrig_beat_num > 0) {
    if (bpm_timer_counter % retrig_timer_reset == 0) {
      if (retrig_ready) {
        if (retrig_first) {
          int r = random_integer_in_range(1, 6);
          if (r < 2) {
            retrig_vol = 1;
          } else if (r == 3) {
            retrig_vol = 0.5;
          } else {
            retrig_vol = 0;
          }
        }
        retrig_beat_num--;
        if (retrig_beat_num == 0) {
          retrig_ready = false;
          retrig_vol = 1.0;
        }
        if (retrig_vol < 1.0) {
          retrig_vol += retrig_vol_step;
          if (retrig_vol > 1.0) {
            retrig_vol = 1.0;
          }
        }
        if (fil_is_open && debounce_quantize == 0) {
          do_update_phase_from_beat_current();
          // mem_use = true;
        }
        retrig_first = false;
      }
    }
  } else if (dub_step_break > -1) {
    if (bpm_timer_counter % (192 * dub_step_numerator[dub_step_divider] /
                             dub_step_denominator[dub_step_divider]) ==
        0) {
      dub_step_break++;
      if (dub_step_break == dub_step_steps[dub_step_divider]) {
        dub_step_divider++;
        dub_step_break = 0;
        if (dub_step_divider == 5) {
          dub_step_break = -1;
        }
      }
      beat_current = dub_step_beat;
      printf("[dub_step_break] beat_current: %d\n", beat_current);
      // debounce a little bit before going into the mode
      if (dub_step_divider > 0 || dub_step_break > 1) {
        // printf("dub: %d %d %d\n", dub_step_break, dub_step_divider,
        //        bpm_timer_counter);
        do_update_phase_from_beat_current();
        printf("%d %ld\n", phase_new, time_us_32());
      }
    }
  } else if (toggle_chain_play) {
    int8_t beat = Chain_emit(chain, bpm_timer_counter);
    if (beat > -1) {
      printf("[toggle_chain_play] beat: %d\n", beat);
      beat_current = beat;
      LEDS_clearAll(leds, LED_STEP_FACE);
      LEDS_set(leds, LED_STEP_FACE, beat_current % 16 + 4, 1);
      do_update_phase_from_beat_current();
    }
  } else if (banks[sel_bank_cur]
                     ->sample[sel_sample_cur]
                     .snd[sel_variation]
                     ->splice_trigger > 0
             // do not iterate the beat if we are in a timestretched variation,
             // let it roll
             && sel_variation == 0) {
    retrig_vol = 1.0;
    if (bpm_timer_counter % banks[sel_bank_cur]
                                ->sample[sel_sample_cur]
                                .snd[sel_variation]
                                ->splice_trigger ==
        0) {
      mem_use = false;
      // keep to the beat
      if (fil_is_open && debounce_quantize == 0) {
        if (beat_current == 0 && !phase_forward) {
          beat_current = banks[sel_bank_cur]
                             ->sample[sel_sample_cur]
                             .snd[sel_variation]
                             ->slice_num;
        } else {
          beat_current += (phase_forward * 2 - 1);
        }
        beat_total++;
        if (sf->pattern_on && sf->pattern_length[sf->pattern_current] > 0) {
          beat_current =
              sf->pattern_sequence[sf->pattern_current]
                                  [beat_total %
                                   sf->pattern_length[sf->pattern_current]];
        }
        // int8_t step_pressed = single_step_pressed();
        // if (step_pressed > -1) {
        //   beat_current = step_pressed % banks[sel_bank_cur]
        //                                     ->sample[sel_sample_cur]
        //                                     .snd[sel_variation]
        //                                     ->slice_num;
        //   printf("[step_pressed] beat_current: %d\n", beat_current);
        // }
        // printf("beat_current: %d\n", beat_current);
        LEDS_clearAll(leds, LED_STEP_FACE);
        LEDS_set(leds, LED_STEP_FACE, beat_current % 16 + 4, 1);
        if (key_jump_debounce == 0) {
          do_update_phase_from_beat_current();
        } else {
          key_jump_debounce--;
        }
      }
      if (debounce_quantize > 0) {
        debounce_quantize--;
      }
    }
  }
  // Charlieplex_toggle(cp, beat_current % 16);
  // printf("Repeat at %lld\n", time_us_64());
  return true;
}

uint16_t freqs_available[74] = {
    200,   250,   262,   277,   294,   311,   330,   349,  370,  392,  415,
    440,   466,   494,   523,   554,   587,   622,   659,  698,  740,  784,
    831,   880,   932,   988,   1047,  1109,  1175,  1245, 1319, 1397, 1480,
    1568,  1661,  1760,  1865,  1976,  2093,  2217,  2349, 2489, 2637, 2794,
    2960,  3136,  3322,  3520,  3729,  3951,  4186,  4435, 4699, 4978, 5274,
    5588,  5920,  6272,  6645,  7040,  7459,  7902,  8372, 8870, 9397, 9956,
    10548, 11175, 11840, 12544, 13290, 14080, 14917, 15804};

void input_handling() {
  printf("core1 running!\n");
  // flash bad signs
  while (!fil_is_open) {
    printf("waiting to start\n");
    sleep_ms(10);
  }
  LEDS_clearAll(leds, 2);
  LEDS_render(leds);

  ButtonMatrix *bm;
  // initialize button matrix
  bm = ButtonMatrix_create(BTN_ROW_START, BTN_COL_START);

  printf("entering while loop\n");

  uint pressed2 = 0;
  uint8_t new_vol;
  //   (
  // a=Array.fill(72,{ arg i;
  //   (i+60).midicps.round.asInteger
  // });
  // a.postln;
  // )

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
    adc_select_input(0);
    int adc;

    // knob X
    adc = FilterExp_update(adcs[0], adc_read());
    if (abs(adc_last[0] - adc) > adc_threshold) {
      adc_debounce[0] = adc_debounce_max;
    }
    if (adc_debounce[0] > 0) {
      adc_last[0] = adc;
      adc_debounce[0]--;
      // TODO: keep track of old value and
      if (button_is_pressed(KEY_SHIFT)) {
        sf->bpm_tempo = adc * 50 / 4096 * 5 + 50;
      } else if (button_is_pressed(KEY_A)) {
        gate_threshold = adc *
                         (30 * (44100 / SAMPLES_PER_BUFFER) / sf->bpm_tempo) /
                         4096 * 2;
      } else if (button_is_pressed(KEY_B)) {
      } else if (button_is_pressed(KEY_C)) {
      }
    }

    button_handler(bm);

    // knob Y
    adc_select_input(1);
    adc = FilterExp_update(adcs[1], adc_read());
    if (abs(adc_last[1] - adc) > adc_threshold) {
      adc_debounce[1] = adc_debounce_max;
    }
    if (adc_debounce[1] > 0) {
      adc_last[1] = adc;
      adc_debounce[1]--;
      if (button_is_pressed(KEY_SHIFT)) {
      } else if (button_is_pressed(KEY_A)) {
        for (uint8_t channel = 0; channel < 2; channel++) {
          if (adc < 2000) {
            ResonantFilter_setFilterType(resFilter[channel], 0);
            ResonantFilter_setFc(resFilter[channel],
                                 adc * (resonantfilter_fc_max) / 2000);
          } else if (adc > 2096) {
            ResonantFilter_setFilterType(resFilter[channel], 1);
            ResonantFilter_setFc(resFilter[channel],
                                 (adc - 2096) * (resonantfilter_fc_max) / 2000);
          } else {
            ResonantFilter_setFilterType(resFilter[channel], 0);
            ResonantFilter_setFc(resFilter[channel], resonantfilter_fc_max);
          }
        }
      } else if (button_is_pressed(KEY_B)) {
      } else if (button_is_pressed(KEY_C)) {
      }
    }

    // knob Z
    adc_select_input(2);
    adc = FilterExp_update(adcs[2], adc_read());
    if (abs(adc_last[2] - adc) > adc_threshold) {
      adc_debounce[2] = adc_debounce_max;
    }
    if (adc_debounce[2] > 0) {
      adc_last[2] = adc;
      adc_debounce[2]--;
      if (button_is_pressed(KEY_SHIFT)) {
        new_vol = adc * MAX_VOLUME / 4096;
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
      }
    }

    LEDS_render(leds);
  }
}

int main() {
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

  sleep_ms(100);

  // run multi core
  // multicore_launch_core1(core1_main);

  // DCDC PSM control
  // 0: PFM mode (best efficiency)
  // 1: PWM mode (improved ripple)
  gpio_init(PIN_DCDC_PSM_CTRL);
  gpio_set_dir(PIN_DCDC_PSM_CTRL, GPIO_OUT);
  gpio_put(PIN_DCDC_PSM_CTRL, 1);  // PWM mode for less Audio noise

  // test stuff
  // sleep_ms(3000);
  // printf("hello\n");
  // uint32_t t0, t1;
  // t0 = time_us_32();
  // envelope_pitch = Envelope2_create(441, 0.5, 1.0, 1.5);
  // int32_t v = Q16_16_1;
  // uint32_t iterations = 441;
  // for (uint32_t i = 0; i < iterations; i++) {
  //   v = q16_16_multiply(q16_16_float_to_fp(1.231),
  //                       Envelope2_update(envelope_pitch));
  // }
  // t1 = time_us_32();
  // printf("it/s: %2.3f\n", (float)iterations / (float)(t1 - t0) * 1000000.0);
  // sleep_ms(300000000);

  ap = init_audio();

  // load new save file
  sf = SaveFile_New();

  // Implicitly called by disk_initialize,
  // but called here to set up the GPIOs
  // before enabling the card detect interrupt:
  sd_init_driver();

  // initialize adcs
  adc_init();
  adc_gpio_init(26);
  adc_gpio_init(27);
  adc_gpio_init(28);

  // init timers
  // Negative delay so means we will call repeating_timer_callback, and call
  // it again 500ms later regardless of how long the callback took to execute
  // add_repeating_timer_ms(-1000, repeating_timer_callback, NULL, &timer);
  // cancel_repeating_timer(&timer);
  add_repeating_timer_us(-(round(30000000 / sf->bpm_tempo / 96)),
                         repeating_timer_callback, NULL, &timer);

  // Loop forever doing nothing
  printf("-/+ to change volume");

  // initialize random library
  random_initialize();

  // initialize sequencers
  chain = Chain_create();

  leds = LEDS_create();
  // show X in case the files aren't loaded
  LEDS_show_blinking_z(leds, 2);

  sleep_ms(1000);
  printf("startup!\n");
  sdcard_startup();

  // TODO
  // load chain from SD card
  //   Chain_load(chain, &sync_using_sdcard);

#ifdef INCLUDE_FILTER
  resFilter[0] = ResonantFilter_create(0);
  resFilter[1] = ResonantFilter_create(0);
#endif
#ifdef INCLUDE_RGBLED
  ws2812 = WS2812_new(23, pio0, 2);
  sleep_ms(1);
  WS2812_fill(ws2812, 0, 0, 0);
  sleep_ms(1);
  WS2812_show(ws2812);
  // for (uint8_t i = 0; i < 255; i++) {
  //   WS2812_fill(ws2812, i, 0, 0);
  //   WS2812_show(ws2812);
  //   sleep_ms(4);
  // }
  // for (uint8_t i = 0; i < 255; i++) {
  //   WS2812_fill(ws2812, 0, i, 0);
  //   WS2812_show(ws2812);
  //   sleep_ms(4);
  // }
  // for (uint8_t i = 0; i < 255; i++) {
  //   WS2812_fill(ws2812, 0, 0, i);
  //   WS2812_show(ws2812);
  //   sleep_ms(4);
  // }
  // WS2812_fill(ws2812, 20, 20, 0);
  // WS2812_show(ws2812);
#endif

  sel_sample_next = 0;
  sel_variation_next = 0;
  sel_bank_cur = 0;
  sel_sample_cur = 0;
  sel_variation = 0;
  fil_current_change = true;

  // blocking
  input_handling();
  // while (true) {
  //   run_keyboard();
  // }
}
