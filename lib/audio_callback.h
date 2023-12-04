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
uint8_t cpu_usage_flag = 0;
bool audio_was_cpu_muted = false;
const uint8_t cpu_usage_flag_limit = 2;
const uint8_t cpu_usage_limit_threshold = 80;

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
      // printf("reduce_cpu_usage: %d\n", reduce_cpu_usage);
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
        printf("next file: %s\n", banks[sel_bank_next]
                                      ->sample[sel_sample_next]
                                      .snd[sel_variation_next]
                                      ->name);
        phase_new = round(((float)phases[0] * (float)banks[sel_bank_next]
                                                  ->sample[sel_sample_next]
                                                  .snd[sel_variation_next]
                                                  ->size) /
                          (float)banks[sel_bank_cur]
                              ->sample[sel_sample_cur]
                              .snd[sel_variation]
                              ->size);

        printf("phase[0] -> phase_new: %d*%d/%d -> %d\n", phases[0],
               banks[sel_bank_next]
                   ->sample[sel_sample_next]
                   .snd[sel_variation_next]
                   ->size,
               banks[sel_bank_cur]
                   ->sample[sel_sample_cur]
                   .snd[sel_variation]
                   ->size,
               phase_new);
        printf("beat_current -> new beat_current: %d", beat_current);
        beat_current =
            round(((float)beat_current * (float)banks[sel_bank_next]
                                             ->sample[sel_sample_next]
                                             .snd[sel_variation_next]
                                             ->slice_num)) /
            (float)banks[sel_bank_cur]
                ->sample[sel_sample_cur]
                .snd[sel_variation]
                ->slice_num;
        printf(" -> %d\n", beat_current);
        phase_change = true;
        do_open_file = true;
      }
    }

    // determine the samples to read
    envelope_pitch_val = Envelope2_update(envelope_pitch);
    // TODO: switch for if wobble is enabled
    // envelope_pitch_val =
    //     envelope_pitch_val * Range(LFNoise2(noise_wobble, 1), 0.9, 1.1);

    // TODO: check if tempo matching is activated, if not then don't change
    // based on bpm
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

    if (!phase_change) {
      int32_t next_phase = phases[0] + values_to_read * (phase_forward * 2 - 1);
      int32_t splice_start = banks[sel_bank_cur]
                                 ->sample[sel_sample_cur]
                                 .snd[sel_variation]
                                 ->slice_start[banks[sel_bank_cur]
                                                   ->sample[sel_sample_cur]
                                                   .snd[sel_variation]
                                                   ->slice_current];
      int32_t splice_stop = banks[sel_bank_cur]
                                ->sample[sel_sample_cur]
                                .snd[sel_variation]
                                ->splice_stop[banks[sel_bank_cur]
                                                  ->sample[sel_sample_cur]
                                                  .snd[sel_variation]
                                                  ->slice_current];
      int32_t sample_stop =
          banks[sel_bank_cur]->sample[sel_sample_cur].snd[sel_variation]->size;
      switch (banks[sel_bank_cur]
                  ->sample[sel_sample_cur]
                  .snd[sel_variation]
                  ->play_mode) {
        case PLAY_NORMAL:
          if (phase_forward && phase[0] > sample_stop) {
            phase_change = true;
            phase_new = phase[0] - sample_stop;
          } else if (!phase_forward && phase[0] < 0) {
            phase_change = true;
            phase_new = phase[0] + sample_stop;
          }
          break;
        case PLAY_SPLICE_STOP:
          if ((phase_forward && (next_phase > splice_stop)) ||
              (!phase_forward && (next_phase < splice_start))) {
            do_fade_out = true;
          }
        case PLAY_SPLICE_LOOP:
          if (phase_forward && (phase[0] > splice_stop) {
            phase_change = true;
            phase_new = splice_start;
          } else if (!phase_forward && (phase[0] < splice_stop) {
            phase_change = true;
            phase_new = splice_stop;
          }
        case default:
          break;
      }
    }

    if (phase_change) {
      do_crossfade = true;
      phases[1] = phases[0];  // old phase
      phase_change = false;
    }

    if (audio_was_muted || audio_was_cpu_muted) {
      audio_was_muted = false;
      audio_was_cpu_muted = false;
      do_fade_in = true;
      // if fading in then do not crossfade
      do_crossfade = false;
    }
    // cpu_usage_flag is written when cpu usage is consistently high
    // in which case it will fade out audio and keep it muted for a little
    // bit to reduce cpu usage
    if (cpu_usage_flag == cpu_usage_flag_limit) {
      audio_was_cpu_muted = true;
      do_fade_out = true;
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
      if (phases[head] != last_seeked) {
        if (f_lseek(&fil_current,
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
      }

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

      // saturate before resampling?
      for (uint16_t i = 0; i < values_len; i++) {
        values[i] = transfer_doublesine(values[i]);
      }

      if (banks[sel_bank_cur]
              ->sample[sel_sample_cur]
              .snd[sel_variation]
              ->num_channels == 1) {
        // mono
        int16_t *newArray;
        if (quadratic_resampling) {
          newArray = array_resample_quadratic_fp(values, samples_to_read,
                                                 buffer->max_sample_count);
        } else {
          newArray = array_resample_linear(values, samples_to_read,
                                           buffer->max_sample_count);
        }

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

          int16_t *newArray;
          if (quadratic_resampling) {
            newArray = array_resample_quadratic_fp(valuesC, samples_to_read,
                                                   buffer->max_sample_count);
          } else {
            newArray = array_resample_linear(valuesC, samples_to_read,
                                             buffer->max_sample_count);
          }

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

// apply filter
#ifdef INCLUDE_FILTER
  for (uint16_t i = 0; i < buffer->max_sample_count; i++) {
    for (uint8_t channel = 0; channel < 2; channel++) {
      samples[i * 2 + channel] =
          ResonantFilter_update(resFilter[channel], samples[i * 2 + channel]);
      if (banks[sel_bank_cur]
              ->sample[sel_sample_cur]
              .snd[sel_variation]
              ->num_channels == 1) {
        samples[i * 2 + 1] = samples[i * 2 + 0];
        break;
      }
    }
  }
#endif

  // for (uint16_t i = 0; i < buffer->max_sample_count; i++) {
  //   samples[i * 2 + 0] =
  //       q16_16_sin(q16_16_multiply(
  //           Q16_16_2PI, q16_16_divide(samples[i * 2 + 0], Q16_16_MAX)))
  //       << 15;
  //   samples[i * 2 + 1] =
  //       q16_16_sin(q16_16_multiply(
  //           Q16_16_2PI, q16_16_divide(samples[i * 2 + 1], Q16_16_MAX)))
  //       << 15;
  // }

  buffer->sample_count = buffer->max_sample_count;
  give_audio_buffer(ap, buffer);

  if (do_fade_out) {
    audio_mute = true;
  }
  if (fil_is_open) {
    for (uint8_t head = 0; head < 2; head++) {
      if ((int64_t)phases[head] >= (int64_t)banks[sel_bank_cur]
                                       ->sample[sel_sample_cur]
                                       .snd[sel_variation]
                                       ->size &&
          banks[sel_bank_cur]
                  ->sample[sel_sample_cur]
                  .snd[sel_variation]
                  ->play_mode != PLAY_SPLICE_STOP) {
        printf("> phase_forward: %d\n", phase_forward);
        printf("> phase[head]: %d\n", phases[head]);
        printf("> size: %d\n", banks[sel_bank_cur]
                                   ->sample[sel_sample_cur]
                                   .snd[sel_variation]
                                   ->size);
        printf("> size: %d\n", banks[0]->sample[0].snd[0]->size);
        // TODO: check playback type
        if (phase_forward) {
          // going forward, restart from the beginning
          phases[head] -= banks[sel_bank_cur]
                              ->sample[sel_sample_cur]
                              .snd[sel_variation]
                              ->size;
        }
      } else if (phases[head] < 0 && banks[sel_bank_cur]
                                             ->sample[sel_sample_cur]
                                             .snd[sel_variation]
                                             ->play_mode != PLAY_SPLICE_STOP) {
        printf("< phase_forward: %d\n", phase_forward);
        printf("< phase[head]: %d\n", phases[head]);
        if (phase_forward == 0) {
          // going backwards, restart from the end
          printf("restart from the end!!!!\n");
          phases[head] += banks[sel_bank_cur]
                              ->sample[sel_sample_cur]
                              .snd[sel_variation]
                              ->size;
        }
      }
    }
  }
  sync_using_sdcard = false;

  clock_t endTime = time_us_64();
  cpu_utilizations[cpu_utilizations_i] =
      100 * (endTime - startTime) / (US_PER_BLOCK);
  cpu_utilizations_i++;

  if (cpu_utilizations_i == 64) {
    uint16_t cpu_utilization = 0;
    for (uint8_t i = 0; i < 64; i++) {
      cpu_utilization = cpu_utilization + cpu_utilizations[i];
    }
#ifdef PRINT_AUDIO_CPU_USAGE
    printf("average cpu utilization: %2.1f\n", ((float)cpu_utilization) / 64.0);
#endif
    cpu_utilizations_i = 0;
#ifdef PRINT_SDCARD_TIMING
    printf("%ld\n", t1 - t0);
#endif
  }
  if (cpu_usage_flag == cpu_usage_flag_limit) {
    cpu_usage_flag = 0;
    reduce_cpu_usage = 10;
  } else {
    if (cpu_utilizations[cpu_utilizations_i] > cpu_usage_limit_threshold) {
      // reduce_cpu_usage = reduce_cpu_usage + 5;
      cpu_usage_flag++;
      // printf("cpu utilization: %d, flag: %d\n",
      //        cpu_utilizations[cpu_utilizations_i], cpu_usage_flag);
    } else {
      cpu_usage_flag = 0;
    }
  }

  return;
}
