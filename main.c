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
          phase_new =
              (file_list[fil_current_bank].size[fil_current_id]) *
              ((beat_current %
                (2 * file_list[fil_current_bank].beats[fil_current_id])) +
               (1 - phase_forward)) /
              (2 * file_list[fil_current_bank].beats[fil_current_id]);
          phase_change = true;
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
        PCA9552_clear(pca);
        PCA9552_ledSet(pca, beat_current % 16, 2);
        EnvelopeGate_reset(envelopegate, BLOCKS_PER_SECOND, 1, 0, 0.05, 0.1);
        phase_new = (file_list[fil_current_bank].size[fil_current_id]) *
                    ((beat_current %
                      (2 * file_list[fil_current_bank].beats[fil_current_id])) +
                     (1 - phase_forward)) /
                    (2 * file_list[fil_current_bank].beats[fil_current_id]);
        phase_change = true;
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

void core1_main() {
  sleep_ms(100);
  printf("core1 running!\n");

  ButtonMatrix *bm;
  // initialize button matrix
  bm = ButtonMatrix_create(1, 6);

  ButtonMatrix_read(bm);
  if (bm->changed) {
    for (uint8_t i = 0; i < bm->num_pressed; i++) {
      printf("%d ", bm->on[i]);
    }
    printf("\n");
  }

  printf("entering while loop\n");
  uint pressed2 = 0;
  uint8_t new_vol;
  while (1) {
    adc_select_input(0);
    sleep_ms(1);

    sf->bpm_tempo = adc_read() * 10 / 4096 * 25 + 50;
    // printf(" adc_read(): %d\n", adc_read());

    adc_select_input(2);
    sleep_ms(1);
    new_vol = (adc_read() * (MAX_VOLUME / 5) / 4096) * 5;
    if (new_vol != sf->vol) {
      sf->vol = new_vol;
      printf("sf-vol: %d\n", sf->vol);
    }

    ButtonMatrix_read(bm);
    if (bm->changed) {
      for (uint8_t i = 0; i < bm->num_pressed; i++) {
        printf("%d ", bm->on[i]);
      }
      printf("\n");
      if (bm->changed_on) {
        if (bm->num_pressed == 2 && bm->on[0] == KEY_A && bm->on[1] >= 4) {
          // switch sample to the one in the current bank
          fil_current_bank_next = fil_current_bank_sel;
          fil_current_id_next =
              (bm->on[1] - 4) % file_list[fil_current_bank_next].num;
          printf("fil_current_bank_next = %d\n", fil_current_bank_next);
          printf("fil_current_id_next = %d\n", fil_current_id_next);
          fil_current_change = true;
        } else if (bm->num_pressed == 2 && bm->on[0] == KEY_B &&
                   bm->on[1] >= 4) {
          // switch bank if the bank has more than one zero files
          if (file_list[bm->on[1] - 4].num > 0) {
            fil_current_bank_sel = bm->on[1] - 4;
          }
        } else {
          pressed2 = 0;
          if (bm->num_pressed == 1 || bm->num_pressed == 2) {
            uint8_t key = bm->on[bm->num_pressed - 1];
            // if (key == KEY_SHIFT) {
            //   Envelope2_reset(envelope_pitch, BLOCKS_PER_SECOND,
            //                   Envelope2_update(envelope_pitch), 1.0, 1);
            //   debounce_quantize = 2;
            // } else if (key == 1) {
            //   debounce_quantize = 2;
            //   Envelope2_reset(envelope_pitch, BLOCKS_PER_SECOND,
            //                   Envelope2_update(envelope_pitch), 0.5, 1);
            // } else if (key == 2) {
            //   phase_forward = !phase_forward;
            // }
            if (key >= 4) {
              beat_current = (beat_current / 16) * 16 + (key - 4);
              PCA9552_clear(pca);
              PCA9552_ledSet(pca, beat_current % 16, 2);

              PCA9552_render(pca);
              phase_new = (file_list[fil_current_bank].size[fil_current_id]) *
                          bm->on[bm->num_pressed - 1] / 16;
              phase_new = (phase_new / 4) * 4;
              phase_change = true;
              debounce_quantize = 2;
            }
          }
        }
      }
    } else {
      if (bm->num_pressed == 2 && pressed2 < 10) {
        if (bm->on[0] > 3 && bm->on[1] > 3) {
          pressed2++;
          if (pressed2 == 10) {
            printf("debounce 2press\n");
            debounce_quantize = 0;
            retrig_first = true;
            retrig_beat_num = random_integer_in_range(8, 24);
            retrig_timer_reset = 96 * random_integer_in_range(1, 4) /
                                 random_integer_in_range(2, 12);
            float total_time =
                (float)(retrig_beat_num * retrig_timer_reset * 60) /
                (float)(96 * sf->bpm_tempo);
            if (total_time > 2.0f) {
              total_time = total_time / 2;
              retrig_timer_reset = retrig_timer_reset / 2;
            }
            if (total_time > 2.0f) {
              total_time = total_time / 2;
              retrig_beat_num = retrig_beat_num / 2;
              if (retrig_beat_num == 0) {
                retrig_beat_num = 1;
              }
            }
            retrig_vol_step = 1.0 / ((float)retrig_beat_num);
            printf(
                "retrig_beat_num=%d,retrig_timer_reset=%d,total_time=%2.3fs\n",
                retrig_beat_num, retrig_timer_reset, total_time);
            retrig_ready = true;
          }
        }
      }
    }
    PCA9552_render(pca);
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

  // setup leds
  i2c_init(i2c_default, 40 * 1000);
  gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
  gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
  gpio_pull_up(I2C_SDA_PIN);
  gpio_pull_up(I2C_SCL_PIN);
  pca = PCA9552_create(0x60, i2c_default);
  if (pca->error != PCA9552_OK) {
    printf("PCA9552_ERROR: %02x\n", pca->error);
  }
  PCA9552_clear(pca);

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

  sleep_ms(100);
  sdcard_startup();

#ifdef INCLUDE_FILTER
  myFilter0 = IIR_new(7000.0f, 0.707f, 1.0f, 44100.0f);
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

  // debug stuff
  fil_current_id_next = 1;
  fil_current_change = true;

  // blocking
  while (true) {
    run_keyboard();
  }
}

// audio callback
void i2s_callback_func2() {
  Envelope2_reset(envelope1, BLOCKS_PER_SECOND, 0, 1.0, 0.04);
  audio_buffer_t *buffer = take_audio_buffer(ap, false);
  if (buffer == NULL) {
    return;
  }
  int32_t *samples = (int32_t *)buffer->buffer->bytes;
  for (uint i = 0; i < buffer->max_sample_count; i++) {
    int32_t value0 = 0;
    int32_t value1 = 0;
    // use 32bit full scale
    samples[i * 2 + 0] = value0 + (value0 >> 16u);  // L
    samples[i * 2 + 1] = value1 + (value1 >> 16u);  // R
  }
  buffer->sample_count = buffer->max_sample_count;
  give_audio_buffer(ap, buffer);
  return;
}

void i2s_callback_func() {
  clock_t startTime = time_us_64();
  audio_buffer_t *buffer = take_audio_buffer(ap, false);
  if (buffer == NULL) {
    return;
  }
  int32_t *samples = (int32_t *)buffer->buffer->bytes;

  if (sync_using_sdcard || !fil_is_open) {
    for (uint16_t i = 0; i < buffer->max_sample_count; i++) {
      int32_t value0 = 0;
      samples[i * 2 + 0] = value0 + (value0 >> 16u);  // L
      samples[i * 2 + 1] = samples[i * 2 + 0];        // R = L
    }
    buffer->sample_count = buffer->max_sample_count;
    give_audio_buffer(ap, buffer);
    if (fil_is_open) {
      printf("[i2s_callback_func] sync_using_sdcard being used\n");
    }
    return;
  }

  // mutex
  sync_using_sdcard = true;

  if (fil_is_open) {
    // check if the file is the right one
    if (fil_current_change) {
      fil_current_change = false;
      if (fil_current_bank != fil_current_bank_next ||
          fil_current_id != fil_current_id_next) {
        // printf("phases[0]: %d\n", phases[0]);
        // printf("fil_current_bank_next: %d\n", fil_current_bank_next);
        // printf("fil_current_id_next: %d\n", fil_current_id_next);
        // printf(
        //     "file_list[fil_current_bank_next].size[fil_current_id_next]:
        //     %d\n",
        //     file_list[fil_current_bank_next].size[fil_current_id_next]);
        // printf("file_list[fil_current_bank].size[fil_current_id]: %d\n",
        //        file_list[fil_current_bank].size[fil_current_id]);
        // printf(
        //     "file_list[fil_current_bank_next].size[fil_current_id_next] / "
        //     "file_list[fil_current_bank].size[fil_current_id]: %d\n",
        //     file_list[fil_current_bank_next].size[fil_current_id_next] /
        //         file_list[fil_current_bank].size[fil_current_id]);
        phases[0] = phases[0] *
                    file_list[fil_current_bank_next].size[fil_current_id_next] /
                    file_list[fil_current_bank].size[fil_current_id];
        // printf("phases[0]: %d\n", phases[0]);
        FRESULT fr;
        fr = f_close(&fil_current);  // close and re-open trick
        printf("f_close = %d\n", fr);
        fr = f_open(&fil_current,
                    file_list[fil_current_bank_next].name[fil_current_id_next],
                    FA_READ);
        printf("f_open = %d\n", fr);
        fr = f_lseek(
            &fil_current,
            WAV_HEADER_SIZE + (phases[0] / PHASE_DIVISOR) * PHASE_DIVISOR);
        printf("f_lseek = %d\n", fr);
        fil_current_id = fil_current_id_next;
        fil_current_bank = fil_current_bank_next;
      }
      fil_current_change = false;
    }

    // flag for new phase
    if (phase_change) {
      phases_since_last[0] = 0;
      phases_since_last[1] = 0;

      phases[1] = phases[0];  // old phase
      phases[0] = (phase_new / PHASE_DIVISOR) * PHASE_DIVISOR;
      phase_change = false;
      // initiate transition envelopes
      // jump point envelope grows
      Envelope2_reset(envelope1, BLOCKS_PER_SECOND, 0, 1.0, 0.04);
      // previous point degrades
      Envelope2_reset(envelope2, BLOCKS_PER_SECOND, 1.0, 0, 0.04);
    }

    envelope_pitch_val = Envelope2_update(envelope_pitch);
    // TODO: switch for if wobble is enabled
    // envelope_pitch_val =
    //     envelope_pitch_val * Range(LFNoise2(noise_wobble, 1), 0.9, 1.1);

    uint32_t samples_to_read = buffer->max_sample_count *
                               round(sf->bpm_tempo * envelope_pitch_val) /
                               file_list[fil_current_bank].bpm[fil_current_id];
    uint32_t values_len = samples_to_read * WAV_CHANNELS;
    uint32_t values_to_read = samples_to_read * WAV_CHANNELS * 2;
    int16_t values[values_len];
    uint vol_main =
        (uint)round(sf->vol * retrig_vol * Envelope2_update(envelope3));

    for (uint8_t head = 0; head < 2; head++) {
      if (head == 1 && phases_since_last[0] >= CROSSFADE_MAX) {
        continue;
      }

      if (!mem_use) {
        if (f_lseek(&fil_current,
                    WAV_HEADER_SIZE +
                        (phases[head] / PHASE_DIVISOR) * PHASE_DIVISOR)) {
          printf("problem seeking to phase (%d)\n", phases[head]);
          for (uint16_t i = 0; i < buffer->max_sample_count; i++) {
            int32_t value0 = 0;
            samples[i * 2 + 0] = value0 + (value0 >> 16u);  // L
            samples[i * 2 + 1] = samples[i * 2 + 0];        // R = L
          }
          buffer->sample_count = buffer->max_sample_count;
          give_audio_buffer(ap, buffer);
          sync_using_sdcard = false;
          sdcard_startup();
          return;
        }

        if (f_read(&fil_current, values, values_to_read, &fil_bytes_read)) {
          printf("ERROR READING!\n");
          f_close(&fil_current);  // close and re-open trick
          f_open(&fil_current, file_list[fil_current_bank].name[fil_current_id],
                 FA_READ);
          f_lseek(
              &fil_current,
              WAV_HEADER_SIZE + (phases[head] / PHASE_DIVISOR) * PHASE_DIVISOR);
        }
        if (fil_bytes_read < values_to_read) {
          // printf("asked for %d bytes, read %d bytes\n", values_to_read,
          //        fil_bytes_read);
          if (f_lseek(&fil_current, WAV_HEADER_SIZE)) {
            printf("problem seeking to 0\n");
          }
          int16_t values2[values_to_read - fil_bytes_read];  // max limit
          if (f_read(&fil_current, values2, values_to_read - fil_bytes_read,
                     &fil_bytes_read2)) {
            printf("ERROR READING!\n");
            f_close(&fil_current);  // close and re-open trick
            f_open(&fil_current,
                   file_list[fil_current_bank].name[fil_current_id], FA_READ);
            f_lseek(&fil_current,
                    WAV_HEADER_SIZE +
                        (phases[head] / PHASE_DIVISOR) * PHASE_DIVISOR);
          }
          // printf("asked for %d bytes, read %d bytes\n",
          //        values_to_read - fil_bytes_read, fil_bytes_read2);
          for (uint16_t i = 0; i < fil_bytes_read2 / 2; i++) {
            values[i + fil_bytes_read / 2] = values2[i];
          }
        }

        if (!phase_forward) {
          // reverse audio
          for (int i = 0; i < values_len / 2; i++) {
            int16_t temp = values[i];
            values[i] = values[values_len - i - 1];
            values[values_len - i - 1] = temp;
          }
        }
      }

      // // save to memory
      // if (mem_use) {
      //   for (int i = 0; i < values_len; i++) {
      //     values[i] = mem_samples[head][mem_index[head]];
      //     mem_index[head]++;
      //   }
      // } else {
      //   for (int i = 0; i < values_len; i++) {
      //     if (mem_index[head] < 44100) {
      //       mem_samples[head][mem_index[head]] = values[i];
      //       mem_index[head]++;
      //     }
      //   }
      // }

#ifndef INCLUDE_STEREO
      int16_t *newArray = array_resample_linear(values, samples_to_read,
                                                buffer->max_sample_count);
      for (uint16_t i = 0; i < buffer->max_sample_count; i++) {
        if (head == 0) {
          samples[i * 2 + 0] = 0;
        }

        uint vol = vol_main;
        if (phases_since_last[head] < CROSSFADE_MAX) {
          if (head == 0) {
            vol = vol_main - crossfade_vol(vol_main, phases_since_last[head]);
          } else {
            vol = crossfade_vol(vol_main, phases_since_last[head]);
            // if (phases_since_last[head] % CROSSFADE_UPDATE_SAMPLES == 0) {
            //   printf("head1 vol: %d\n", vol);
            // }
          }
          phases_since_last[head]++;
        }

        newArray[i] = transfer_fn(newArray[i]);
        int32_t value0 = (vol * newArray[i]) << 8u;
#ifdef INCLUDE_FILTER
        IIR_filter(myFilter0, &value0);
#endif
        samples[i * 2 + 0] =
            samples[i * 2 + 0] + value0 + (value0 >> 16u);  // L
        samples[i * 2 + 1] = samples[i * 2 + 0];            // R = L
      }
      free(newArray);
#endif
#ifdef INCLUDE_STEREO
      // stereo
      int16_t valuesL[samples_to_read];  // max limit
      int16_t valuesR[samples_to_read];  // max limit
      for (uint16_t i = 0; i < samples_to_read * WAV_CHANNELS; i++) {
        if (i % 2 == 0) {
          valuesL[i / 2] = values[i];
        } else {
          valuesR[i / 2] = values[i];
        }
      }
      int16_t *newArrayL = array_resample_linear(valuesL, samples_to_read,
                                                 buffer->max_sample_count);
      int16_t *newArrayR = array_resample_linear(valuesR, samples_to_read,
                                                 buffer->max_sample_count);

      for (uint16_t i = 0; i < buffer->max_sample_count; i++) {
        if (head == 0) {
          samples[i * 2 + 0] = 0;
          samples[i * 2 + 1] = 0;
        }

        uint vol = vol_main;
        if (phases_since_last[head] < CROSSFADE_MAX) {
          if (head == 0) {
            vol = vol_main - crossfade_vol(vol_main, phases_since_last[head]);
          } else {
            vol = crossfade_vol(vol_main, phases_since_last[head]);
          }
          phases_since_last[head]++;
        }

        newArrayL[i] = transfer_fn(newArrayL[i]);
        int32_t value0 = (vol * newArrayL[i]) << 8u;
#ifdef INCLUDE_FILTER
        IIR_filter(myFilter0, &value0);
#endif
        newArrayR[i] = transfer_fn(newArrayR[i]);
        int32_t value1 = (vol * newArrayR[i]) << 8u;
#ifdef INCLUDE_FILTER
        IIR_filter(myFilter1, &value1);
#endif
        samples[i * 2 + 0] =
            samples[i * 2 + 0] + value0 + (value0 >> 16u);  // L
        samples[i * 2 + 1] =
            samples[i * 2 + 1] + value1 + (value1 >> 16u);  // L
      }
      free(newArrayL);
      free(newArrayR);
#endif
      phases[head] += values_to_read * (phase_forward * 2 - 1);
      phases_old[head] = phases[head];
    }
#ifdef INCLUDE_BASS
    // add bass
    Bass_callback(bass, samples, buffer->max_sample_count);
#endif
  }

  buffer->sample_count = buffer->max_sample_count;
  give_audio_buffer(ap, buffer);

  if (fil_is_open) {
    for (uint8_t head = 0; head < 2; head++) {
      if (phases[head] >= file_list[fil_current_bank].size[fil_current_id]) {
        phases[head] -= file_list[fil_current_bank].size[fil_current_id];
      } else if (phases[head] < 0) {
        phases[head] += file_list[fil_current_bank].size[fil_current_id];
      }
    }
  }
  sync_using_sdcard = false;

  clock_t endTime = time_us_64();
  cpu_utilization = 100 * (endTime - startTime) / (US_PER_BLOCK);
  return;
}
