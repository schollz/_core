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

uint8_t cpu_utilizations[64];
uint8_t cpu_utilizations_i = 0;
uint32_t last_seeked = 1;
uint32_t reduce_cpu_usage = 0;

bool audio_was_muted = false;

// ignore boundaries
#define PLAY_NORMAL 0
// starts at splice start and ends at splice stop
#define PLAY_SPLICE_STOP 1
// starts at splice start, and returns to start when reaching splice boundary
#define PLAY_SPLICE_LOOP 2
// starts at splice start and ends at sample boundary
#define PLAY_SAMPLE_STOP 3
// starts at splice start and returns to start when reaching sample boundary
#define PLAY_SAMPLE_LOOP 4

void i2s_callback_func() {
  uint32_t t0, t1;
  uint8_t sd_calls = 0;
#ifdef PRINT_AUDIO_CPU_USAGE
  clock_t startTime = time_us_64();
#endif
  audio_buffer_t *buffer = take_audio_buffer(ap, false);
  if (buffer == NULL) {
    return;
  }
  int32_t *samples = (int32_t *)buffer->buffer->bytes;

  if (sync_using_sdcard || !fil_is_open ||
      (gate_active && gate_counter >= gate_threshold) || audio_mute ||
      reduce_cpu_usage > 0) {
    if (reduce_cpu_usage > 0) {
      reduce_cpu_usage--;
    }
    for (uint16_t i = 0; i < buffer->max_sample_count; i++) {
      int32_t value0 = 0;
      samples[i * 2 + 0] = value0 + (value0 >> 16u);  // L
      samples[i * 2 + 1] = samples[i * 2 + 0];        // R = L
    }
    buffer->sample_count = buffer->max_sample_count;
    give_audio_buffer(ap, buffer);
    // if (!gate_active && fil_is_open && !audio_mute) {
    //   printf("[i2s_callback_func] sync_using_sdcard being used\n");
    // }
    if (audio_mute) {
      audio_was_muted = true;
    }
    return;
  }

  // mutex
  sync_using_sdcard = true;

  if (fil_is_open) {
    // gating
    bool do_gate_up = false;
    bool do_gate_down = false;
    if (gate_is_applied && gate_counter == 0) {
      gate_is_applied = false;
      do_gate_up = true;  // allow the sound to come through
    } else if (gate_active) {
      gate_counter++;
      if (!gate_is_applied && gate_counter >= gate_threshold) {
        do_gate_down = true;  // mute the sound
      }
    }

    bool do_open_file = false;
    // check if the file is the right one
    if (fil_current_change) {
      fil_current_change = false;
      if (sel_bank_cur != sel_bank_next || sel_sample_cur != sel_sample_next ||
          sel_variation != sel_variation_next) {
        phase_change = true;
        phase_new = round((float)phases[0] /
                          (float)banks[sel_bank_cur]
                              ->sample[sel_sample_cur]
                              .snd[sel_variation]
                              ->size *
                          (float)banks[sel_bank_next]
                              ->sample[sel_sample_next]
                              .snd[sel_variation_next]
                              ->size);
        phase_new = (phase_new / PHASE_DIVISOR) * PHASE_DIVISOR;
        beat_current = round((float)beat_current /
                             (float)banks[sel_bank_cur]
                                 ->sample[sel_sample_cur]
                                 .snd[sel_variation]
                                 ->slice_num *
                             (float)banks[sel_bank_next]
                                 ->sample[sel_sample_next]
                                 .snd[sel_variation_next]
                                 ->slice_num);
        printf("next file: %s\n", banks[sel_bank_next]
                                      ->sample[sel_sample_next]
                                      .snd[sel_variation_next]
                                      ->name);
        do_open_file = true;
      }
    }

    // determine the samples to read
    envelope_pitch_val = Envelope2_update(envelope_pitch);
    // TODO: switch for if wobble is enabled
    // envelope_pitch_val =
    //     envelope_pitch_val * Range(LFNoise2(noise_wobble, 1), 0.9, 1.1);
    uint32_t samples_to_read =
        buffer->max_sample_count * round(sf->bpm_tempo * envelope_pitch_val) /
        banks[sel_bank_cur]->sample[sel_sample_cur].snd[sel_variation]->bpm *
        banks[sel_bank_cur]
            ->sample[sel_sample_cur]
            .snd[sel_variation]
            ->oversampling;
    uint32_t values_len = samples_to_read * banks[sel_bank_cur]
                                                ->sample[sel_sample_cur]
                                                .snd[sel_variation]
                                                ->num_channels;
    uint32_t values_to_read = values_len * 2;  // 16-bit = 2 x 1 byte reads
    int16_t values[values_len];
    uint vol_main =
        (uint)round(sf->vol * retrig_vol * Envelope2_update(envelope3));

    // flag for new phase
    bool do_crossfade = false;
    bool do_fade_out = false;
    bool do_fade_in = false;
    if (phase_change) {
      do_crossfade = true;
      phases[1] = phases[0];  // old phase
      phases[0] = (phase_new / PHASE_DIVISOR) * PHASE_DIVISOR;
#ifdef INCLUDE_FILTER
      ResonantFilter_copy(resonantfilter[0], resonantfilter[1]);
#endif
      phase_change = false;
    }

    if (banks[sel_bank_cur]
            ->sample[sel_sample_cur]
            .snd[sel_variation]
            ->play_mode == PLAY_SPLICE_STOP) {
      // do a mute once the sample extends past the start or end of the splice
      uint32_t next_phase =
          phases[0] + values_to_read * (phase_forward * 2 - 1);
      if ((phase_forward > 0 &&
           next_phase > banks[sel_bank_cur]
                            ->sample[sel_sample_cur]
                            .snd[sel_variation]
                            ->slice_stop[banks[sel_bank_cur]
                                             ->sample[sel_sample_cur]
                                             .snd[sel_variation]
                                             ->slice_current]) ||
          (phase_forward == 0 &&
           next_phase < banks[sel_bank_cur]
                            ->sample[sel_sample_cur]
                            .snd[sel_variation]
                            ->slice_start[banks[sel_bank_cur]
                                              ->sample[sel_sample_cur]
                                              .snd[sel_variation]
                                              ->slice_current])) {
        do_fade_out = true;
        audio_mute = true;
      }
    } else if (banks[sel_bank_cur]
                   ->sample[sel_sample_cur]
                   .snd[sel_variation]
                   ->play_mode == PLAY_SAMPLE_STOP) {
      // do a mute once the sample extends past the start or end of the sample
      uint32_t next_phase =
          phases[0] + values_to_read * (phase_forward * 2 - 1);
      if ((phase_forward > 0 &&
           next_phase > banks[sel_bank_cur]
                            ->sample[sel_sample_cur]
                            .snd[sel_variation]
                            ->slice_stop[banks[sel_bank_cur]
                                             ->sample[sel_sample_cur]
                                             .snd[sel_variation]
                                             ->slice_num -
                                         1]) ||
          (phase_forward == 0 && next_phase < banks[sel_bank_cur]
                                                  ->sample[sel_sample_cur]
                                                  .snd[sel_variation]
                                                  ->slice_start[0])) {
        do_fade_out = true;
        audio_mute = true;
      }
    }
    if (audio_was_muted) {
      audio_was_muted = false;
      do_fade_in = true;
    }

    bool first_loop = true;
    for (int8_t head = 1; head >= 0; head--) {
      if (head == 1 && !do_crossfade) {
        continue;
      }

      if (head == 0 && do_open_file) {
        do_open_file = false;
        // setup the next
        sel_sample_cur = sel_sample_next;
        sel_bank_cur = sel_bank_next;
        sel_variation = sel_variation_next;

        FRESULT fr;
        fr = f_close(&fil_current);
        if (fr != FR_OK) {
          debugf("[audio_callback] f_close error: %s\n", FRESULT_str(fr));
        }
        fr = f_open(&fil_current,
                    banks[sel_bank_cur]
                        ->sample[sel_sample_cur]
                        .snd[sel_variation]
                        ->name,
                    FA_READ);
        if (fr != FR_OK) {
          debugf("[audio_callback] f_open error: %s\n", FRESULT_str(fr));
        }
      }

      // optimization here, only seek if the current position is not at the
      // phases[head]
      phases[head] = (phases[head] / PHASE_DIVISOR) * PHASE_DIVISOR;
      if (phases[head] != last_seeked) {
        if (f_lseek(&fil_current, WAV_HEADER +
                                      (banks[sel_bank_cur]
                                           ->sample[sel_sample_cur]
                                           .snd[sel_variation]
                                           ->num_channels *
                                       banks[sel_bank_cur]
                                           ->sample[sel_sample_cur]
                                           .snd[sel_variation]
                                           ->oversampling *
                                       44100) +
                                      phases[head])) {
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
      }

      ++sd_calls;
      t0 = time_us_32();

      if (f_read(&fil_current, values, values_to_read, &fil_bytes_read)) {
        printf("ERROR READING!\n");
        f_close(&fil_current);  // close and re-open trick
        f_open(&fil_current,
               banks[sel_bank_cur]
                   ->sample[sel_sample_cur]
                   .snd[sel_variation]
                   ->name,
               FA_READ);
        f_lseek(&fil_current,
                WAV_HEADER +
                    (banks[sel_bank_cur]
                         ->sample[sel_sample_cur]
                         .snd[sel_variation]
                         ->num_channels *
                     banks[sel_bank_cur]
                         ->sample[sel_sample_cur]
                         .snd[sel_variation]
                         ->oversampling *
                     44100) +
                    (phases[head] / PHASE_DIVISOR) * PHASE_DIVISOR);
      }
      t1 = time_us_32();
      last_seeked = phases[head] + fil_bytes_read;

      if (fil_bytes_read < values_to_read) {
        printf("%d %d: asked for %d bytes, read %d bytes\n", phases[head],
               WAV_HEADER +
                   (banks[sel_bank_cur]
                        ->sample[sel_sample_cur]
                        .snd[sel_variation]
                        ->num_channels *
                    banks[sel_bank_cur]
                        ->sample[sel_sample_cur]
                        .snd[sel_variation]
                        ->oversampling *
                    44100) +
                   phases[head],
               values_to_read, fil_bytes_read);
      }

      if (!phase_forward) {
        // reverse audio
        for (int i = 0; i < values_len / 2; i++) {
          int16_t temp = values[i];
          values[i] = values[values_len - i - 1];
          values[values_len - i - 1] = temp;
        }
      }

      if (banks[sel_bank_cur]
              ->sample[sel_sample_cur]
              .snd[sel_variation]
              ->num_channels == 1) {
        // mono
        int16_t *newArray = array_resample_linear(values, samples_to_read,
                                                  buffer->max_sample_count);
        for (uint16_t i = 0; i < buffer->max_sample_count; i++) {
          if (do_crossfade) {
            if (head == 0 && !do_fade_out) {
              newArray[i] = crossfade3_in(newArray[i], i, CROSSFADE3_COS);
            } else if (!do_fade_in) {
              newArray[i] = crossfade3_out(newArray[i], i, CROSSFADE3_COS);
            }
          } else if (do_fade_out) {
            newArray[i] = crossfade3_out(newArray[i], i, CROSSFADE3_COS);
          } else if (do_fade_in) {
            newArray[i] = crossfade3_in(newArray[i], i, CROSSFADE3_COS);
          }

          if (do_gate_down) {
            // mute the audio
            newArray[i] = crossfade3_out(newArray[i], i, CROSSFADE3_COS);
          } else if (do_gate_up) {
            // bring back the audio
            newArray[i] = crossfade3_in(newArray[i], i, CROSSFADE3_COS);
          }

          if (first_loop) {
            samples[i * 2 + 0] = newArray[i];
            if (head == 0) {
              samples[i * 2 + 0] = (vol_main * samples[i * 2 + 0]) << 8u;
              samples[i * 2 + 0] += (samples[i * 2 + 0] >> 16u);
              samples[i * 2 + 1] = samples[i * 2 + 0];  // R = L
            }
          } else {
            samples[i * 2 + 0] += newArray[i];
            samples[i * 2 + 0] = (vol_main * samples[i * 2 + 0]) << 8u;
            samples[i * 2 + 0] += (samples[i * 2 + 0] >> 16u);
            samples[i * 2 + 1] = samples[i * 2 + 0];  // R = L
          }
          // int32_t value0 = (vol * newArray[i]) << 8u;
          // samples[i * 2 + 0] =
          //     samples[i * 2 + 0] + value0 + (value0 >> 16u);  // L
        }
        if (first_loop) {
          first_loop = false;
        }
        free(newArray);
      } else if (banks[sel_bank_cur]
                     ->sample[sel_sample_cur]
                     .snd[sel_variation]
                     ->num_channels == 2) {
        // stereo
        for (uint8_t channel = 0; channel < 2; channel++) {
          int16_t valuesC[samples_to_read];  // max limit
          for (uint16_t i = 0; i < values_len; i++) {
            if (i % 2 == channel) {
              valuesC[i / 2] = values[i];
            }
          }
          int16_t *newArray = array_resample_linear(valuesC, samples_to_read,
                                                    buffer->max_sample_count);

          for (uint16_t i = 0; i < buffer->max_sample_count; i++) {
            if (first_loop) {
              samples[i * 2 + channel] = 0;
            }

            if (do_crossfade) {
              if (head == 0 && !do_fade_out) {
                newArray[i] = crossfade3_in(newArray[i], i, CROSSFADE3_COS);
              } else if (!do_fade_in) {
                newArray[i] = crossfade3_out(newArray[i], i, CROSSFADE3_COS);
              }
            } else if (do_fade_out) {
              newArray[i] = crossfade3_out(newArray[i], i, CROSSFADE3_COS);
            } else if (do_fade_in) {
              newArray[i] = crossfade3_in(newArray[i], i, CROSSFADE3_COS);
            }

            if (do_gate_down) {
              // mute the audio
              newArray[i] = crossfade3_out(newArray[i], i, CROSSFADE3_COS);
            } else if (do_gate_up) {
              // bring back the audio
              newArray[i] = crossfade3_in(newArray[i], i, CROSSFADE3_COS);
            }

            int32_t value0 = (vol_main * newArray[i]) << 8u;
            samples[i * 2 + channel] += value0 + (value0 >> 16u);
          }
          free(newArray);
        }
        first_loop = false;
      }

      phases[head] += (values_to_read * (phase_forward * 2 - 1));
      phases_old[head] = phases[head];
    }
  }

  buffer->sample_count = buffer->max_sample_count;
  give_audio_buffer(ap, buffer);

  if (fil_is_open) {
    for (uint8_t head = 0; head < 2; head++) {
      if (phases[head] >= banks[sel_bank_cur]
                              ->sample[sel_sample_cur]
                              .snd[sel_variation]
                              ->size) {
        // TODO: check playback type
        if (phase_forward) {
          // going forward, restart from the beginning
          phases[head] -= banks[sel_bank_cur]
                              ->sample[sel_sample_cur]
                              .snd[sel_variation]
                              ->size;
        }
      } else if (phases[head] < 0) {
        if (phase_forward == 0) {
          // going backwards, restart from the end
          phases[head] += banks[sel_bank_cur]
                              ->sample[sel_sample_cur]
                              .snd[sel_variation]
                              ->size;
        }
      }
    }
  }
  sync_using_sdcard = false;

#ifdef PRINT_AUDIO_USAGE
  clock_t endTime = time_us_64();
#ifdef PRINT_AUDIO_CPU_USAGE
  cpu_utilizations[cpu_utilizations_i] =
      100 * (endTime - startTime) / (US_PER_BLOCK);
#endif
  cpu_utilizations_i++;

  if (cpu_utilizations_i == 64) {
    uint16_t cpu_utilization = 0;
    for (uint8_t i = 0; i < 64; i++) {
      cpu_utilization = cpu_utilization + cpu_utilizations[i];
    }
#ifdef PRINT_AUDIO_CPU_USAGE
    printf("average cpu utilization: %2.1f\n", sd_calls,
           ((float)cpu_utilization) / 64.0);
#endif
    cpu_utilizations_i = 0;
    printf("%ld\n", t1 - t0);
  }
  if (cpu_utilizations[cpu_utilizations_i] > 70) {
    printf("cpu utilization: %d\n", cpu_utilizations[cpu_utilizations_i]);
    reduce_cpu_usage = reduce_cpu_usage + 1;
  }
#endif

  return;
}
