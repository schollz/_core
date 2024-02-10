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
  if (do_restart_playback) {
    do_restart_playback = false;
    playback_restarted = true;
    bpm_timer_counter = -1;
    bpm_timer_counter_last = bpm_timer_counter;
    beat_total = -1;
    key_jump_debounce = 0;
    dub_step_break = -1;
    retrig_beat_num = 0;
    beat_current = -1;
    playback_stopped = false;
    // restart the sequencers
    for (uint8_t i = 0; i < 3; i++) {
      if (sequencerhandler[i].playing) {
        Sequencer_play(sf->sequencers[i][sf->sequence_sel[i]], true);
      }
    }
  }
  if (do_stop_playback) {
    beat_current = 0;
    do_stop_playback = false;
    playback_stopped = true;
  }
  if (playback_stopped) {
    return true;
  }

  bpm_timer_counter++;
  bool do_splice_trigger = (bpm_timer_counter % (banks[sel_bank_cur]
                                                     ->sample[sel_sample_cur]
                                                     .snd[sel_variation]
                                                     ->splice_trigger)) == 0;
  // trigger clock out if it is going
  if (do_splice_trigger) {
    clock_out_ready = true;
    clock_did_activate = true;
  }
  for (uint8_t i = 0; i < 3; i++) {
    if (sequencerhandler[i].playing) {
      Sequencer_step(sf->sequencers[i][sf->sequence_sel[i]], bpm_timer_counter);
    }
  }
  if (retrig_beat_num > 0) {
    if (bpm_timer_counter % retrig_timer_reset == 0) {
      if (retrig_ready) {
        if (retrig_first) {
          retrig_vol =
              linlin((float)sf->fx_param[FX_RETRIGGER][0], 0, 255, 0, 1);
          retrig_pitch = PITCH_VAL_MID;
          if (sf->fx_active[FX_RETRIGGER]) {
            retrig_pitch_change =
                linlin(sf->fx_param[FX_RETRIGGER][1], 0, 255, 1, 5);
            if (random_integer_in_range(1, 10) < 5) {
              retrig_pitch_change = -1 * retrig_pitch_change;
            }
          } else {
            retrig_pitch_change = 0;
          }
        }
        retrig_beat_num--;
        if (retrig_beat_num == 0) {
          retrig_ready = false;
          retrig_vol = 1.0;
          retrig_pitch = PITCH_VAL_MID;
        }
        if (retrig_vol < 1.0) {
          retrig_vol += retrig_vol_step;
          if (retrig_vol > 1.0) {
            retrig_vol = 1.0;
          }
        }
        if (retrig_pitch > 0 && retrig_pitch < PITCH_VAL_MAX - 1) {
          retrig_pitch += retrig_pitch_change;
        }
        if (fil_is_open && debounce_quantize == 0) {
          do_update_phase_from_beat_current();
          // mem_use = true;
        }
        retrig_first = false;
      }
    }
  } else if (sequencerhandler[0].playing &&
             banks[sel_bank_cur]
                     ->sample[sel_sample_cur]
                     .snd[sel_variation]
                     ->play_mode != PLAY_NORMAL) {
  } else if (((!banks[sel_bank_cur]
                    ->sample[sel_sample_cur]
                    .snd[sel_variation]
                    ->one_shot &&
               !clock_in_do) ||
              (clock_in_ready && clock_in_do))
             // TODO if splice_trigger is 0, but we are sequencing, then need to
             // continue here!

             // do not iterate the beat if we are in a timestretched variation,
             // let it roll
             && sel_variation == 0) {
    retrig_vol = 1.0;
    retrig_pitch = PITCH_VAL_MID;
    retrig_pitch_change = 0;

    if (banks[sel_bank_cur]
            ->sample[sel_sample_cur]
            .snd[sel_variation]
            ->splice_variable > 0) {
      // calculate the size of this slice in pulses
      float num_slices = (float)(banks[sel_bank_cur]
                                     ->sample[sel_sample_cur]
                                     .snd[sel_variation]
                                     ->slice_stop[banks[sel_bank_cur]
                                                      ->sample[sel_sample_cur]
                                                      .snd[sel_variation]
                                                      ->slice_current] -
                                 banks[sel_bank_cur]
                                     ->sample[sel_sample_cur]
                                     .snd[sel_variation]
                                     ->slice_start[banks[sel_bank_cur]
                                                       ->sample[sel_sample_cur]
                                                       .snd[sel_variation]
                                                       ->slice_current]);
      num_slices = round(
          num_slices /
          (88200.0 * (banks[sel_bank_cur]
                          ->sample[sel_sample_cur]
                          .snd[sel_variation]
                          ->num_channels +
                      1)) *
          banks[sel_bank_cur]->sample[sel_sample_cur].snd[sel_variation]->bpm /
          60.0 * 192.0);
      do_splice_trigger =
          (bpm_timer_counter - bpm_timer_counter_last) >= num_slices;
      if (do_splice_trigger) {
        printf("do_splice_trigger: %d %2.0f %d %2.0f\n",
               banks[sel_bank_cur]
                   ->sample[sel_sample_cur]
                   .snd[sel_variation]
                   ->slice_current,
               num_slices, bpm_timer_counter, (float)bpm_timer_counter_last);
        bpm_timer_counter_last = bpm_timer_counter;
      }
    }

    if (sequencerhandler[0].playing) {
      // already done
    } else if ((clock_in_do && clock_in_ready) || do_splice_trigger) {
      clock_in_ready = false;
      mem_use = false;
      // keep to the beat
      if (fil_is_open && debounce_quantize == 0) {
        beat_total++;
        if (clock_in_do) {
          beat_current = clock_in_beat_total % banks[sel_bank_cur]
                                                   ->sample[sel_sample_cur]
                                                   .snd[sel_variation]
                                                   ->slice_num;
        } else if (key3_activated && mode_buttons16 == MODE_JUMP) {
          uint8_t lo = key3_pressed_keys[0] - 4;
          uint8_t hi = key3_pressed_keys[1] - 4;
          if (lo > hi) {
            uint8_t tmp = lo;
            lo = hi;
            hi = tmp;
            beat_current--;
          } else {
            beat_current++;
          }
          if (beat_current > hi) {
            beat_current = lo;
          } else if (beat_current < lo) {
            beat_current = hi;
          }
          beat_current = beat_current % banks[sel_bank_cur]
                                            ->sample[sel_sample_cur]
                                            .snd[sel_variation]
                                            ->slice_num;
        } else {
          if (beat_current == 0 && !phase_forward) {
            beat_current = banks[sel_bank_cur]
                               ->sample[sel_sample_cur]
                               .snd[sel_variation]
                               ->slice_num;
          } else {
            beat_current += (phase_forward * 2 - 1);
          }
          if (beat_current < 0) {
            beat_current += banks[sel_bank_cur]
                                ->sample[sel_sample_cur]
                                .snd[sel_variation]
                                ->slice_num;
          }
          if (only_play_kicks) {
            // check if beat_current is a kick, if not try to find one
            for (uint16_t i = 0; i < banks[sel_bank_cur]
                                         ->sample[sel_sample_cur]
                                         .snd[sel_variation]
                                         ->slice_num;
                 i++) {
              uint16_t j = beat_current + i;
              if (j > banks[sel_bank_cur]
                          ->sample[sel_sample_cur]
                          .snd[sel_variation]
                          ->slice_num) {
                j -= banks[sel_bank_cur]
                         ->sample[sel_sample_cur]
                         .snd[sel_variation]
                         ->slice_num;
              }
              if (banks[sel_bank_cur]
                          ->sample[sel_sample_cur]
                          .snd[sel_variation]
                          ->slice_type[j] == 1 ||
                  banks[sel_bank_cur]
                          ->sample[sel_sample_cur]
                          .snd[sel_variation]
                          ->slice_type[j] == 3) {
                beat_current = j;
                break;
              }
            }
          } else if (only_play_snares) {
            // check if beat_current is a kick, if not try to find one
            for (uint16_t i = 0; i < banks[sel_bank_cur]
                                         ->sample[sel_sample_cur]
                                         .snd[sel_variation]
                                         ->slice_num;
                 i++) {
              uint16_t j = beat_current + i;
              if (j > banks[sel_bank_cur]
                          ->sample[sel_sample_cur]
                          .snd[sel_variation]
                          ->slice_num) {
                j -= banks[sel_bank_cur]
                         ->sample[sel_sample_cur]
                         .snd[sel_variation]
                         ->slice_num;
              }
              if (banks[sel_bank_cur]
                          ->sample[sel_sample_cur]
                          .snd[sel_variation]
                          ->slice_type[j] == 2 ||
                  banks[sel_bank_cur]
                          ->sample[sel_sample_cur]
                          .snd[sel_variation]
                          ->slice_type[j] == 3) {
                beat_current = j;
                break;
              }
            }
          }
        }
        // printf("beat_current: %d\n", beat_current);
        if (key_jump_debounce == 0 && !sf->fx_active[FX_SCRATCH]) {
          // printf("[main] beat_current: %d, beat_total: %d\n", beat_current,
          //        beat_total);
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
  // update lfos
  lfo_pan_val += lfo_pan_step;
  if (lfo_pan_val > Q16_16_2PI) {
    lfo_pan_val -= Q16_16_2PI;
  }
  lfo_tremelo_val += lfo_tremelo_step;
  if (lfo_tremelo_val > Q16_16_2PI) {
    lfo_tremelo_val -= Q16_16_2PI;
  }

  return true;
}

#ifdef INCLUDE_ZEPTOCORE
#include "lib/zeptocore.h"
#endif
#ifdef INCLUDE_ECTOCORE
#include "lib/ectocore.h"
#endif
#ifdef INCLUDE_BOARDCORE
#include "lib/boardcore.h"
#endif

int main() {
  // Set PLL_USB 96MHz
  const uint32_t main_line = 96;
  pll_init(pll_usb, 1, main_line * 16 * MHZ, 4, 4);
  clock_configure(clk_usb, 0, CLOCKS_CLK_USB_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB,
                  main_line * MHZ, main_line / 2 * MHZ);
  // Change clk_sys to be 96MHz.
  clock_configure(clk_sys, CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLKSRC_CLK_SYS_AUX,
                  CLOCKS_CLK_SYS_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB,
                  main_line * MHZ, main_line * MHZ);
  // CLK peri is clocked from clk_sys so need to change clk_peri's freq
  clock_configure(clk_peri, 0, CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLK_SYS,
                  main_line * MHZ, main_line * MHZ);
  // Reinit uart now that clk_peri has changed
  stdio_init_all();
// overclocking!!!
// note that overclocking >200Mhz requires setting sd_card_sdio
// rp2040_sdio_init(sd_card_p, 2);
// otherwise clock divider of 1 is fine
// set_sys_clock_khz(270000, true);
#ifdef DO_OVERCLOCK
  set_sys_clock_khz(225000, true);
#else
  set_sys_clock_khz(125000, true);
#endif
  sleep_ms(100);

  // DCDC PSM control
  // 0: PFM mode (best efficiency)
  // 1: PWM mode (improved ripple)
  gpio_init(PIN_DCDC_PSM_CTRL);
  gpio_set_dir(PIN_DCDC_PSM_CTRL, GPIO_OUT);
  gpio_put(PIN_DCDC_PSM_CTRL, 1);  // PWM mode for less Audio noise

  ap = init_audio();

  // Implicitly called by disk_initialize,
  // but called here to set up the GPIOs
  // before enabling the card detect interrupt:
  sd_init_driver();

#ifdef INCLUDE_ZEPTOCORE
  // initialize adcs
  adc_init();
  adc_gpio_init(26);
  adc_gpio_init(27);
  adc_gpio_init(28);
#endif

  // init timers
  // Negative delay so means we will call repeating_timer_callback, and call
  // it again 500ms later regardless of how long the callback took to execute
  // add_repeating_timer_ms(-1000, repeating_timer_callback, NULL, &timer);
  // cancel_repeating_timer(&timer);
  add_repeating_timer_us(-(round(30000000 / sf->bpm_tempo / 96)),
                         repeating_timer_callback, NULL, &timer);

  // initialize random library
  random_initialize();

  // initialize message sync
  messagesync = MessageSync_malloc();

  // intialize beat repeater
  beatrepeat = BeatRepeat_malloc();

  // initialize delay
  delay = Delay_malloc();
  Delay_setActive(delay, false);
  Delay_setDuration(delay, 8018);

  // initialize saturation
  saturation = Saturation_malloc();

  // initialize debouncers
  for (uint8_t i = 0; i < DEBOUNCE_UINT8_NUM; i++) {
    debouncer_uint8[i] = DebounceUint8_malloc();
  }

#ifdef INCLUDE_ZEPTOCORE
  debouncer_digits = DebounceDigits_malloc();
  leds = LEDS_create();
  ledtext = LEDText_create();
#endif

#ifdef INCLUDE_SINEBASS
  // init_sinewaves();
  wavebass = WaveBass_malloc();
#endif

  // intialize tap tempo
  taptempo = TapTempo_malloc();

  // LEDText_display(ledtext, "HELLO");
  // show X in case the files aren't loaded
  // LEDS_show_blinking_z(leds, 2);

  // printf("startup!\n");
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

#ifdef INCLUDE_ZEPTOCORE

  mode_buttons16 = MODE_JUMP;

  // EnvelopeLinearInteger_reset(
  //     envelope_filter, BLOCKS_PER_SECOND,
  //     EnvelopeLinearInteger_update(envelope_filter, NULL), 5, 1.618);
  // sf->fx_active[FX_FUZZ] = true;
  // sf->fx_active[FX_SATURATE] = true;
  // sf->fx_active[FX_SHAPER] = true;

#endif

  // blocking
  input_handling();
}
