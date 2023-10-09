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
          retrig_vol = 0;
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
          EnvelopeGate_reset(envelopegate, BLOCKS_PER_SECOND, 1, 0,
                             30 / (float)sf->bpm_tempo,
                             30 / (float)sf->bpm_tempo);
          do_update_phase_from_beat_current();
          // mem_use = true;
        }
        retrig_first = false;
      }
    }
  } else {
    if (bpm_timer_counter % bpm_timer_reset == 0) {
      mem_use = false;
      // keep to the beat
      if (fil_is_open && debounce_quantize == 0) {
        if (beat_current == 0 && !phase_forward) {
          beat_current = file_list[fil_current_bank].beats[fil_current_id];
        }
        beat_current += (phase_forward * 2 - 1);
        beat_total++;
        if (sf->pattern_on && sf->pattern_length[sf->pattern_current] > 0) {
          beat_current =
              sf->pattern_sequence[sf->pattern_current]
                                  [beat_total %
                                   sf->pattern_length[sf->pattern_current]];
        }
        // printf("beat_current: %d\n", beat_current);
        LEDS_clearAll(leds, LED_STEP_FACE);
        LEDS_set(leds, LED_STEP_FACE, beat_current % 16 + 4, 1);
        EnvelopeGate_reset(envelopegate, BLOCKS_PER_SECOND, 1, 0, 0.05, 0.1);
        do_update_phase_from_beat_current();
      }
      if (debounce_quantize > 0) {
        debounce_quantize--;
      }
    }
  }
  Charlieplex_toggle(cp, beat_current % 16);
  // printf("Repeat at %lld\n", time_us_64());
  return true;
}

uint16_t freqs_available[72] = {
    262,   277,   294,   311,   330,   349,  370,  392,  415,  440,   466,
    494,   523,   554,   587,   622,   659,  698,  740,  784,  831,   880,
    932,   988,   1047,  1109,  1175,  1245, 1319, 1397, 1480, 1568,  1661,
    1760,  1865,  1976,  2093,  2217,  2349, 2489, 2637, 2794, 2960,  3136,
    3322,  3520,  3729,  3951,  4186,  4435, 4699, 4978, 5274, 5588,  5920,
    6272,  6645,  7040,  7459,  7902,  8372, 8870, 9397, 9956, 10548, 11175,
    11840, 12544, 13290, 14080, 14917, 15804};

void core1_main() {
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
  uint8_t filter_midi = 10;
  //   (
  // a=Array.fill(72,{ arg i;
  //   (i+60).midicps.round.asInteger
  // });
  // a.postln;
  // )
  while (1) {
    adc_select_input(0);
    sleep_ms(1);
    // sf->distortion_level = 3;
    // sf->distortion_wet = adc_read() * 100 / 4096;

    // sf->bpm_tempo = adc_read() * 10 / 4096 * 25 + 50;
    // printf(" adc_read(): %d\n", adc_read());

    adc_select_input(1);
    // sf->saturate_wet = adc_read() * 100 / 4096;
    // sleep_ms(100);
    // printf(" adc_read(): %d\n", adc_read() * 71 / 4096);
    // uint8_t filter_midi_new = adc_read() * 71 / 4096;
    // if (filter_midi != filter_midi_new) {
    //   filter_midi = filter_midi_new;
    //   printf("freqs_available[%d]: %d", filter_midi,
    //          freqs_available[filter_midi]);
    //   IIR_set_fc(myFilter0, freqs_available[filter_midi]);
    // }

    adc_select_input(2);
    sleep_ms(1);
    new_vol = (adc_read() * (MAX_VOLUME / 5) / 4096) * 5;
    if (new_vol != sf->vol) {
      sf->vol = new_vol;
      printf("sf-vol: %d\n", sf->vol);
    }

    button_handler(bm);
    // // keep track of button states
    // bool button_state[20];
    // for (uint8_t i = 0; i < 20; i++) {
    //   button_state[i] = false;
    // }
    // if (bm->changed) {
    //   LEDS_clearAll(leds, LED_PRESS_FACE);
    //   uint8_t keys_shifted[bm->num_pressed];
    //   uint8_t keys_shifted_num = 0;
    //   bool keys_shift_found = false;
    //   for (uint8_t i = 0; i < bm->num_pressed; i++) {
    //     // printf("%d ", bm->on[i]);
    //     LEDS_set(leds, LED_PRESS_FACE, bm->on[i], 2);
    //     if (keys_shift_found) {
    //       keys_shifted[keys_shifted_num] = bm->on[i];
    //       keys_shifted_num++;
    //     }
    //     if (bm->on[i] == KEY_SHIFT) {
    //       keys_shift_found = true;
    //     }
    //   }
    //   if (keys_shifted_num > 0) {
    //     printf("found shift!\n");
    //     for (uint8_t i = 0; i < keys_shifted_num; i++) {
    //       printf("shifted: %d\n", keys_shifted[i]);
    //     }
    //     printf("\nok\n");
    //   } else {
    //     // TODO: cancel all properties that need shift
    //   }

    //   printf("\n");
    //   if (bm->num_pressed > 1 && bm->on[bm->num_pressed - 2] == KEY_SHIFT &&
    //       bm->on[bm->num_pressed - 1] == 19) {
    //     printf("STRETCH!!!\n");

    //   } else if (bm->changed_on) {
    //     if (bm->num_pressed == 2 && bm->on[0] == KEY_A && bm->on[1] >= 4) {
    //       // switch sample to the one in the current bank
    //       fil_current_bank_next = fil_current_bank_sel;
    //       fil_current_id_next =
    //           ((bm->on[1] - 4) % (file_list[fil_current_bank_next].num / 2))
    //           * 2;
    //       printf("fil_current_bank_next = %d\n", fil_current_bank_next);
    //       printf("fil_current_id_next = %d\n", fil_current_id_next);
    //       fil_current_change = true;
    //     } else if (bm->num_pressed == 2 && bm->on[0] == KEY_B &&
    //                bm->on[1] >= 4) {
    //       // switch bank if the bank has more than one zero files
    //       if (file_list[bm->on[1] - 4].num > 0) {
    //         fil_current_bank_sel = bm->on[1] - 4;
    //       }
    //     } else {
    //       pressed2 = 0;
    //       if (bm->num_pressed == 1 || bm->num_pressed == 2) {
    //         uint8_t key = bm->on[bm->num_pressed - 1];
    //         // if (key == KEY_SHIFT) {
    //         //   Envelope2_reset(envelope_pitch, BLOCKS_PER_SECOND,
    //         //                   Envelope2_update(envelope_pitch), 1.0, 1);
    //         //   debounce_quantize = 2;
    //         // } else if (key == 1) {
    //         //   debounce_quantize = 2;
    //         //   Envelope2_reset(envelope_pitch, BLOCKS_PER_SECOND,
    //         //                   Envelope2_update(envelope_pitch), 0.5, 1);
    //         // } else if (key == 2) {
    //         //   phase_forward = !phase_forward;
    //         // }
    //         if (key >= 4) {
    //           beat_current = (beat_current / 16) * 16 + (key - 4);
    //           LEDS_clearAll(leds, LED_STEP_FACE);
    //           LEDS_set(leds, LED_STEP_FACE, beat_current % 16 + 4, 2);

    //           phase_new = (file_list[fil_current_bank].size[fil_current_id])
    //           *
    //                       bm->on[bm->num_pressed - 1] / 16;
    //           phase_new = (phase_new / 4) * 4;
    //           phase_change = true;
    //           debounce_quantize = 2;
    //         }
    //       }
    //     }
    //   }
    // } else {
    //   if (bm->num_pressed == 2 && pressed2 < 10) {
    //     if (bm->on[0] > 3 && bm->on[1] > 3) {
    //       pressed2++;
    //       if (pressed2 == 10) {
    //         printf("debounce 2press\n");
    //         debounce_quantize = 0;
    //         retrig_first = true;
    //         retrig_beat_num = random_integer_in_range(8, 24);
    //         retrig_timer_reset = 96 * random_integer_in_range(1, 4) /
    //                              random_integer_in_range(2, 12);
    //         float total_time =
    //             (float)(retrig_beat_num * retrig_timer_reset * 60) /
    //             (float)(96 * sf->bpm_tempo);
    //         if (total_time > 2.0f) {
    //           total_time = total_time / 2;
    //           retrig_timer_reset = retrig_timer_reset / 2;
    //         }
    //         if (total_time > 2.0f) {
    //           total_time = total_time / 2;
    //           retrig_beat_num = retrig_beat_num / 2;
    //           if (retrig_beat_num == 0) {
    //             retrig_beat_num = 1;
    //           }
    //         }
    //         retrig_vol_step = 1.0 / ((float)retrig_beat_num);
    //         printf(
    //             "retrig_beat_num=%d,retrig_timer_reset=%d,total_time=%2.3fs\n",
    //             retrig_beat_num, retrig_timer_reset, total_time);
    //         retrig_ready = true;
    //       }
    //     }
    //   }
    // }
    LEDS_render(leds);
    sleep_ms(1);
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
  multicore_launch_core1(core1_main);

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

  cp = Charlieplex_create();

  leds = LEDS_create();
  // show X in case the files aren't loaded
  LEDS_show_blinking_z(leds, 2);

  sdcard_startup();

#ifdef INCLUDE_FILTER
  myFilter0 = IIR_new(7000.0f, 5.0f, 1.0f, 44100.0f);
  myFilter1 = IIR_new(7200.0f, 0.707f, 1.0f, 44100.0f);
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

  // blocking
  while (true) {
    run_keyboard();
  }
}
