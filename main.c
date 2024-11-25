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

#include "lib/includes.h"

// static uint8_t dub_step_numerator[] = {1, 1, 1, 1, 1, 1, 1, 1};
// static uint8_t dub_step_denominator[] = {2, 3, 4, 8, 8, 12, 12, 16};
// static uint8_t dub_step_steps[] = {8, 12, 16, 32, 16, 16};
bool repeating_timer_callback_playback_stopped = false;
uint64_t repeating_timer_callback_playback_counter = 0;
int32_t phase_sample_old = 0;
// timer

bool timer_step() {
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

#ifdef INCLUDE_ECTOCORE
  uint8_t ecto_do_clock_trig = 0;
#endif

#ifdef INCLUDE_MIDI
  // always send out MIDI clock signal at 24 pulses per quarter note
  repeating_timer_callback_playback_counter++;
  if (repeating_timer_callback_playback_counter % 8 == 0) {
    send_midi_clock();
  }
#endif
  if (playback_stopped) {
    if (!repeating_timer_callback_playback_stopped) {
      repeating_timer_callback_playback_stopped = true;
#ifdef INCLUDE_MIDI
      send_midi_stop();
#endif
    }
    return true;
  }
  if (repeating_timer_callback_playback_stopped) {
    repeating_timer_callback_playback_stopped = false;
#ifdef INCLUDE_MIDI
    send_midi_start();
#endif
  }

  bpm_timer_counter++;

  bool do_splice_trigger = (bpm_timer_counter % (banks[sel_bank_cur]
                                                     ->sample[sel_sample_cur]
                                                     .snd[FILEZERO]
                                                     ->splice_trigger)) == 0;

// ectocore clocking
#ifdef INCLUDE_ECTOCORE
  if (clock_behavior_sync_slice) {
    if (bpm_timer_counter %
            (banks[sel_bank_cur]
                 ->sample[sel_sample_cur]
                 .snd[FILEZERO]
                 ->splice_trigger *
             ectocore_clock_out_divisions[ectocore_clock_selected_division] /
             8) ==
        0) {
      ecto_do_clock_trig++;
      gpio_put(GPIO_CLOCK_OUT, 1);
      clock_output_trig_time = to_ms_since_boot(get_absolute_time());
    } else if (!clock_output_trig &&
               bpm_timer_counter % (banks[sel_bank_cur]
                                        ->sample[sel_sample_cur]
                                        .snd[FILEZERO]
                                        ->splice_trigger *
                                    ectocore_clock_out_divisions
                                        [ectocore_clock_selected_division] /
                                    8 / 2) ==
                   0) {
      // if clock output trig mode is on, then it will be switched off in the
      // main loop
      gpio_put(GPIO_CLOCK_OUT, 0);
    }
  } else {
    if (bpm_timer_counter %
            (192 *
             ectocore_clock_out_divisions[ectocore_clock_selected_division] /
             8) ==
        0) {
      ecto_do_clock_trig++;
      gpio_put(GPIO_CLOCK_OUT, 1);
      clock_output_trig_time = to_ms_since_boot(get_absolute_time());
    } else if (!clock_output_trig &&
               bpm_timer_counter % (192 *
                                    ectocore_clock_out_divisions
                                        [ectocore_clock_selected_division] /
                                    8 / 2) ==
                   0) {
      // if clock output trig mode is on, then it will be switched off in the
      // main loop
      gpio_put(GPIO_CLOCK_OUT, 0);
    }
  }

#endif
  // trigger clock out if it is going
  if (do_splice_trigger) {
    beat_total++;
    beat_did_activate = true;

    // retriggering at end of a phrase
    if (do_retrig_at_end_of_phrase) {
      if (beat_start_retrig == 0) {
#ifdef INCLUDE_ECTOCORE
        beat_start_retrig = random_integer_in_range(2, 8);
#else
        beat_start_retrig = random_integer_in_range(2, 10);
#endif
      }
      if (((beat_start_retrig >= 4 &&
            beat_total % 64 == (64 - beat_start_retrig)) ||
           (beat_start_retrig < 4 &&
            beat_total % 32 == (32 - beat_start_retrig))) &&
          !retrig_ready && !retrig_first) {
        // do retriggering if beat_current is at the end of 32 beats
        uint8_t time_multiplier[6] = {1, 2, 2, 2, 4, 4};
        uint8_t time_multiplier_index = random_integer_in_range(0, 5);
        debounce_quantize = 0;
        retrig_first = true;
        retrig_beat_num =
            beat_start_retrig * time_multiplier[time_multiplier_index];
        retrig_timer_reset = 96 / time_multiplier[time_multiplier_index];
        retrig_vol_step = 1.0 / ((float)retrig_beat_num);
        retrig_ready = true;
        beat_start_retrig = 0;
        do_random_jump = true;
      }
      if (beat_total % 64 == 0 && random_sequence_length > 0) {
        regenerate_random_sequence_arr();
        random_sequence_length = random_integer_in_range(1, 16) * 4;
      }
    }
    Gate_reset(audio_gate);
    clock_out_ready = true;
    clock_did_activate = true;
#ifdef INCLUDE_ECTOCORE
    ecto_do_clock_trig++;
#endif

    // check if need to do tunneling
    // avoid tunneling if we are in a timestretched variation
    if (sel_variation == 0) {
      if (tunneling_is_on > 0) {
        if (tunneling_is_on < 4 && probability_of_random_tunnel > 750) {
          tunneling_is_on++;
        } else if (tunneling_is_on < 3 && probability_of_random_tunnel > 500) {
          tunneling_is_on++;
        } else if (tunneling_is_on < 2) {
          tunneling_is_on++;
        } else if (random_integer_in_range(1, 1000) >
                   probability_of_random_tunnel) {
          // deactivate tunneling
          tunneling_is_on = 0;
          sel_sample_next = tunneling_original_sample;
          printf("%d, tunneling off: %d -> %d\n", probability_of_random_tunnel,
                 sel_sample_cur, sel_sample_next);
          fil_current_change = true;
        }
      } else {
        if (random_integer_in_range(1, 1000) < probability_of_random_tunnel) {
          // activate tunneling
          tunneling_is_on = 1;
          tunneling_original_sample = sel_sample_cur;
          sel_sample_next =
              random_integer_in_range(0, 15) % banks[sel_bank_cur]->num_samples;
          printf("%d tunneling: %d -> %d\n", probability_of_random_tunnel,
                 sel_sample_cur, sel_sample_next);
          fil_current_change = true;
        }
      }
    }
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
          // generate random value between 0 and 1
#ifdef INCLUDE_ECTOCORE
#else
          retrig_vol = (float)random_integer_in_range(0, 50) / 100;
#endif
          retrig_pitch = PITCH_VAL_MID;
          if (sf->do_retrig_pitch_changes &&
              random_integer_in_range(1, 100) < 70) {
            retrig_pitch_change =
                round((float)random_integer_in_range(100, 500) / 100);
            if (random_integer_in_range(1, 10) < 5) {
              retrig_pitch_change = -1 * retrig_pitch_change;
            }
          } else {
            retrig_pitch_change = 0;
          }
          retrig_filter_change = 0;
          if (random_integer_in_range(1, 100) < 35) {
            // create filter ramp
            if (retrig_filter_original == 0) {
              retrig_filter_original = global_filter_index;
            }
            global_filter_index =
                random_integer_in_range(1, global_filter_index / 2);
            retrig_filter_change =
                (retrig_filter_original - global_filter_index) /
                retrig_beat_num;
          }
        }
        retrig_beat_num--;
        if (retrig_beat_num == 0) {
          retrig_ready = false;
          retrig_vol = 1.0;
          retrig_pitch = PITCH_VAL_MID;
          // reset filter
          if (global_filter_index != retrig_filter_original &&
              retrig_filter_original > 0) {
            global_filter_index = retrig_filter_original;
            for (uint8_t channel = 0; channel < 2; channel++) {
              ResonantFilter_setFc(resFilter[channel], global_filter_index);
            }
            retrig_filter_original = 0;
          }
        }
        if (retrig_vol < 1.0) {
          retrig_vol += retrig_vol_step;
          // printf("retrig_vol: %f\n", retrig_vol);
          if (retrig_vol > 1.0) {
            retrig_vol = 1.0;
          }
        }
        if (global_filter_index < retrig_filter_original) {
          global_filter_index += retrig_filter_change;
          if (global_filter_index > retrig_filter_original) {
            global_filter_index = retrig_filter_original;
          }
          for (uint8_t channel = 0; channel < 2; channel++) {
            ResonantFilter_setFilterType(resFilter[channel],
                                         global_filter_lphp);
            ResonantFilter_setFc(resFilter[channel], global_filter_index);
          }
        }
        if (retrig_pitch > 0 && retrig_pitch < PITCH_VAL_MAX - 1) {
          retrig_pitch += retrig_pitch_change;
          if (retrig_pitch < 0) {
            retrig_pitch = 0;
          } else if (retrig_pitch > PITCH_VAL_MAX - 1) {
            retrig_pitch = PITCH_VAL_MAX - 1;
          }
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
                     .snd[FILEZERO]
                     ->play_mode != PLAY_NORMAL) {
  } else if (((!banks[sel_bank_cur]
                    ->sample[sel_sample_cur]
                    .snd[FILEZERO]
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
    // reset filter
    if (global_filter_index != retrig_filter_original &&
        retrig_filter_original > 0) {
      global_filter_index = retrig_filter_original;
      for (uint8_t channel = 0; channel < 2; channel++) {
        ResonantFilter_setFc(resFilter[channel], global_filter_index);
      }
      retrig_filter_original = 0;
    }

    if (banks[sel_bank_cur]
            ->sample[sel_sample_cur]
            .snd[FILEZERO]
            ->splice_variable > 0) {
      // calculate the size of this slice in pulses
      float num_slices = (float)(banks[sel_bank_cur]
                                     ->sample[sel_sample_cur]
                                     .snd[FILEZERO]
                                     ->slice_stop[banks[sel_bank_cur]
                                                      ->sample[sel_sample_cur]
                                                      .snd[FILEZERO]
                                                      ->slice_current] -
                                 banks[sel_bank_cur]
                                     ->sample[sel_sample_cur]
                                     .snd[FILEZERO]
                                     ->slice_start[banks[sel_bank_cur]
                                                       ->sample[sel_sample_cur]
                                                       .snd[FILEZERO]
                                                       ->slice_current]);
      num_slices =
          round(num_slices /
                (88200.0 * (banks[sel_bank_cur]
                                ->sample[sel_sample_cur]
                                .snd[FILEZERO]
                                ->num_channels +
                            1)) *
                banks[sel_bank_cur]->sample[sel_sample_cur].snd[FILEZERO]->bpm /
                60.0 * 192.0);
      do_splice_trigger =
          (bpm_timer_counter - bpm_timer_counter_last) >= num_slices;
      if (do_splice_trigger) {
        // printf("do_splice_trigger: %d %2.0f %d %2.0f\n",
        //        banks[sel_bank_cur]
        //            ->sample[sel_sample_cur]
        //            .snd[FILEZERO]
        //            ->slice_current,
        //        num_slices, bpm_timer_counter, (float)bpm_timer_counter_last);
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
        if (clock_in_do) {
          beat_current = clock_in_beat_total % banks[sel_bank_cur]
                                                   ->sample[sel_sample_cur]
                                                   .snd[FILEZERO]
                                                   ->slice_num;
          // printf("[main] beat_current from clock in: %d\n", beat_current);
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
                                            .snd[FILEZERO]
                                            ->slice_num;
        } else if (sf->stay_in_sync) {
          beat_current = beat_total % banks[sel_bank_cur]
                                          ->sample[sel_sample_cur]
                                          .snd[FILEZERO]
                                          ->slice_num;
          if (!phase_forward) {
            beat_current = banks[sel_bank_cur]
                               ->sample[sel_sample_cur]
                               .snd[FILEZERO]
                               ->slice_num -
                           beat_current;
          }
        } else {
          if (beat_current == 0 && !phase_forward) {
            beat_current = banks[sel_bank_cur]
                               ->sample[sel_sample_cur]
                               .snd[FILEZERO]
                               ->slice_num;
          } else {
            beat_current += (phase_forward * 2 - 1);
          }
          if (beat_current < 0) {
            beat_current += banks[sel_bank_cur]
                                ->sample[sel_sample_cur]
                                .snd[FILEZERO]
                                ->slice_num;
          }

          if (only_play_kicks) {
            // check if beat_current is a kick, if not try to find one
            for (uint16_t i = 0; i < banks[sel_bank_cur]
                                         ->sample[sel_sample_cur]
                                         .snd[FILEZERO]
                                         ->slice_num;
                 i++) {
              uint16_t j = beat_current + i;
              if (j > banks[sel_bank_cur]
                          ->sample[sel_sample_cur]
                          .snd[FILEZERO]
                          ->slice_num) {
                j -= banks[sel_bank_cur]
                         ->sample[sel_sample_cur]
                         .snd[FILEZERO]
                         ->slice_num;
              }
              if (banks[sel_bank_cur]
                          ->sample[sel_sample_cur]
                          .snd[FILEZERO]
                          ->slice_type[j] == 1 ||
                  banks[sel_bank_cur]
                          ->sample[sel_sample_cur]
                          .snd[FILEZERO]
                          ->slice_type[j] == 3) {
                beat_current = j;
                break;
              }
            }
          } else if (only_play_snares) {
            // check if beat_current is a kick, if not try to find one
            for (uint16_t i = 0; i < banks[sel_bank_cur]
                                         ->sample[sel_sample_cur]
                                         .snd[FILEZERO]
                                         ->slice_num;
                 i++) {
              uint16_t j = beat_current + i;
              if (j > banks[sel_bank_cur]
                          ->sample[sel_sample_cur]
                          .snd[FILEZERO]
                          ->slice_num) {
                j -= banks[sel_bank_cur]
                         ->sample[sel_sample_cur]
                         .snd[FILEZERO]
                         ->slice_num;
              }
              if (banks[sel_bank_cur]
                          ->sample[sel_sample_cur]
                          .snd[FILEZERO]
                          ->slice_type[j] == 2 ||
                  banks[sel_bank_cur]
                          ->sample[sel_sample_cur]
                          .snd[FILEZERO]
                          ->slice_type[j] == 3) {
                beat_current = j;
                break;
              }
            }
          }
        }
#ifdef INCLUDE_ECTOCORE
        if (cv_beat_current_override > 0) {
          beat_current = cv_beat_current_override;
          cv_beat_current_override = -1;
          // printf("[main] cv_beat_current_override: %d\n", beat_current);
        }
#endif
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

#ifdef LED_TOP_GPIO
  gpio_put(LED_TOP_GPIO, beat_total % 2 == 0 ? 1 : 0);
#endif

  // check to see if a phase crossed a boundary of a transient
  // only check if not timestretching
  if (sel_variation == 0) {
    int32_t phase_sample = phases[0] / 2 /
                               (banks[sel_bank_cur]
                                    ->sample[sel_sample_cur]
                                    .snd[FILEZERO]
                                    ->num_channels +
                                1) +
                           440;
    if (phase_sample != phase_sample_old) {
      uint16_t transient_nums[3] = {banks[sel_bank_cur]
                                        ->sample[sel_sample_cur]
                                        .snd[FILEZERO]
                                        ->transient_num_1,
                                    banks[sel_bank_cur]
                                        ->sample[sel_sample_cur]
                                        .snd[FILEZERO]
                                        ->transient_num_2,
                                    banks[sel_bank_cur]
                                        ->sample[sel_sample_cur]
                                        .snd[FILEZERO]
                                        ->transient_num_3};
      for (uint8_t i = 0; i < 3; i++) {
        for (uint8_t j = 0; j < transient_nums[i]; j++) {
          if (banks[sel_bank_cur]
                  ->sample[sel_sample_cur]
                  .snd[FILEZERO]
                  ->transients[i][j] == 0) {
            continue;
          }
          if (phase_sample_old < (banks[sel_bank_cur]
                                      ->sample[sel_sample_cur]
                                      .snd[FILEZERO]
                                      ->transients[i][j] *
                                  16) &&
              phase_sample >= (banks[sel_bank_cur]
                                   ->sample[sel_sample_cur]
                                   .snd[FILEZERO]
                                   ->transients[i][j] *
                               16) &&
              (phase_sample - phase_sample_old < 881)) {
#ifdef INCLUDE_ZEPTOCORE
#ifdef INCLUDE_CUEDSOUNDS
            if (do_layer_kicks > -1) {
              if (i == 0) {
                // is kick
                cuedsounds_do_play = do_layer_kicks;
                printf("[globals] kick %d\n", do_layer_kicks);
              }
            }
#endif
#endif
            // transient activated
#ifdef INCLUDE_ECTOCORE
            if (ectocore_trigger_mode == i) {
              ecto_trig_out_last = to_ms_since_boot(get_absolute_time());
              gpio_put(GPIO_TRIG_OUT, 1);
            }
#endif
          }
        }
      }

      phase_sample_old = phase_sample;
    }
#ifdef INCLUDE_ECTOCORE
    // random transients
    if (ectocore_trigger_mode == TRIGGER_MODE_RANDOM) {
      if (to_ms_since_boot(get_absolute_time()) - ecto_trig_out_last > 100 &&
          random_integer_in_range(1, 6000) < 10) {
        ecto_trig_out_last = to_ms_since_boot(get_absolute_time());
        gpio_put(GPIO_TRIG_OUT, 1);
      }
    }
#endif
  }

  return true;
}

bool repeating_timer_callback(struct repeating_timer *t) {
  return timer_step();
}

#ifdef INCLUDE_ZEPTOCORE
#include "lib/zeptocore.h"
#endif
#ifdef INCLUDE_ECTOCORE
#include "lib/ectocore.h"
#endif
#ifdef INCLUDE_BOARDCORE
#include "lib/zeptoboard.h"
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
#ifdef INCLUDE_BOARDCORE
  set_sys_clock_khz(150000, true);
#else
  set_sys_clock_khz(225000, true);
#endif
#else
  set_sys_clock_khz(125000, true);
#endif
  sleep_ms(75);

  // DCDC PSM control
  // 0: PFM mode (best efficiency)
  // 1: PWM mode (improved ripple)
  gpio_init(PIN_DCDC_PSM_CTRL);
  gpio_set_dir(PIN_DCDC_PSM_CTRL, GPIO_OUT);
  gpio_put(PIN_DCDC_PSM_CTRL, 1);  // PWM mode for less Audio noise

#ifdef INCLUDE_ZEPTOCORE
  i2c_init(i2c_default, 50 * 1000);
  gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
  gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
  gpio_pull_up(I2C_SDA_PIN);
  gpio_pull_up(I2C_SCL_PIN);
  sleep_ms(5);
  // detect if 0x61 exists
  uint8_t rxdata;
  int ret = i2c_read_timeout_us(i2c_default, 0x61, &rxdata, 1, false, 2000);
  if (ret < 0) {
    // normal
    is_arcade_box = false;
  } else {
    // arcade box
    is_arcade_box = true;
    led_text_time = 50;
    mcp23017_init(i2c_default, MCP23017_ADDR1);
    mcp23017_init(i2c_default, MCP23017_ADDR2);
    // addr1
    // set all as inputs
    mcp23017_set_dir_gpioa(i2c_default, MCP23017_ADDR1, 0b11111111);
    sleep_ms(1);
    mcp23017_set_dir_gpiob(i2c_default, MCP23017_ADDR1, 0b11111111);
    sleep_ms(1);
    mcp23017_set_pullup_gpioa(i2c_default, MCP23017_ADDR1, 0b11111111);
    sleep_ms(1);
    mcp23017_set_pullup_gpioa(i2c_default, MCP23017_ADDR1, 0b11111111);
    sleep_ms(1);
    mcp23017_set_pullup_gpiob(i2c_default, MCP23017_ADDR1, 0b11111111);
    sleep_ms(1);
    // addr2
    // set B as outputs and A as inputs
    mcp23017_set_dir_gpioa(i2c_default, MCP23017_ADDR2, 0b11111111);
    sleep_ms(1);
    mcp23017_set_dir_gpiob(i2c_default, MCP23017_ADDR2, 0b00000000);
    sleep_ms(1);
    mcp23017_set_pullup_gpioa(i2c_default, MCP23017_ADDR2, 0b11111111);
    sleep_ms(1);
  }
#endif

#ifdef LED_TOP_GPIO
  gpio_init(LED_TOP_GPIO);
  gpio_set_dir(LED_TOP_GPIO, GPIO_OUT);
#endif

#ifdef INCLUDE_ECTOCORE
  gpio_init(GPIO_BTN_BANK);
  gpio_set_dir(GPIO_BTN_BANK, GPIO_IN);
  gpio_pull_up(GPIO_BTN_BANK);
  gpio_init(GPIO_BTN_TAPTEMPO);
  gpio_set_dir(GPIO_BTN_TAPTEMPO, GPIO_IN);
  gpio_pull_up(GPIO_BTN_TAPTEMPO);
  sleep_ms(1);
  do_calibration_mode =
      (gpio_get(GPIO_BTN_BANK) == 0 && gpio_get(GPIO_BTN_TAPTEMPO) == 0);
  if (do_calibration_mode) {
    sleep_ms(500);
    do_calibration_mode =
        do_calibration_mode &&
        (gpio_get(GPIO_BTN_BANK) == 0 && gpio_get(GPIO_BTN_TAPTEMPO) == 0);
  }
#endif
  if (!do_calibration_mode) {
    ap = init_audio();
  }

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

  combfilter = Comb_malloc();
  Comb_setActive(combfilter, false, 10, 10);

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

  sel_sample_next = 0;
  sel_variation_next = 0;
  sel_bank_cur = 0;
  sel_sample_cur = 0;
  sel_variation = 0;

  // printf("startup!\n");
  sdcard_startup();

  // TODO
  // load chain from SD card
  //   Chain_load(chain, &sync_using_sdcard);

#ifdef INCLUDE_FILTER
  resFilter[0] = ResonantFilter_create(0);
  resFilter[1] = ResonantFilter_create(0);
  amigaFilter[0] = ResonantFilter_create(0);
  amigaFilter[1] = ResonantFilter_create(0);
  // set amiga filter to 12khz
  ResonantFilter_setFc(amigaFilter[0], 68);
  ResonantFilter_setFc(amigaFilter[1], 68);
  ResonantFilter_setFilterType(amigaFilter[0], 0);
  ResonantFilter_setFilterType(amigaFilter[1], 0);
#endif
#ifdef INCLUDE_RGBLED
  ws2812 = WS2812_new(23, pio0, 2);
  sleep_ms(1);
  WS2812_fill(ws2812, 0, 0, 0, 0);
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
