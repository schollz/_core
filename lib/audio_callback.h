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
uint32_t cpu_usage_flag_total = 0;
uint8_t cpu_usage_flag = 0;
uint16_t cpu_flag_counter = 0;
const uint8_t cpu_usage_flag_limit = 3;
const uint8_t cpu_usage_limit_threshold = 150;

bool audio_was_muted = false;

void update_filter_from_envelope(int32_t val) {
  for (uint8_t channel = 0; channel < 2; channel++) {
    ResonantFilter_setFilterType(resFilter[channel], 0);
    ResonantFilter_setFc(resFilter[channel], val);
  }
}

void i2s_callback_func() {
  uint32_t values_to_read;
  uint32_t t0, t1;
  uint32_t sd_card_total_time = 0;
  uint32_t give_audio_buffer_time = 0;
  uint32_t take_audio_buffer_time = 0;

  // flag for new phase
  bool do_crossfade = false;
  bool do_fade_out = false;
  bool do_fade_in = false;
  clock_t startTime = time_us_64();
  audio_buffer_t *buffer = take_audio_buffer(ap, false);
  take_audio_buffer_time = (time_us_64() - startTime);
  if (buffer == NULL) {
    return;
  }

  EnvelopeLinearInteger_update(envelope_filter, update_filter_from_envelope);

  float envelope_volume_val = Envelope2_update(envelope_volume);
  float envelope_pitch_val_new = Envelope2_update(envelope_pitch);

  int32_t *samples = (int32_t *)buffer->buffer->bytes;

  if (sync_using_sdcard || !fil_is_open ||
      (gate_active && gate_counter >= gate_threshold) || audio_mute ||
      button_mute || reduce_cpu_usage > 0 ||
      (envelope_pitch_val < ENVELOPE_PITCH_THRESHOLD) ||
      envelope_volume_val < 0.001 || Gate_is_up(audio_gate)) {
    envelope_pitch_val = envelope_pitch_val_new;

    // continue to update the gate
    Gate_update(audio_gate, sf->bpm_tempo);

    // check cpu usage
    if (reduce_cpu_usage > 0) {
      // printf("reduce_cpu_usage: %d\n", reduce_cpu_usage);
      reduce_cpu_usage--;
      if (reduce_cpu_usage == 0) {
        audio_mute = false;
      }
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

    // audio muted flag to ensure a fade in occurs when
    // unmuted
    audio_was_muted = true;
    return;
  }

  Gate_update(audio_gate, sf->bpm_tempo);
  if (Gate_is_up(audio_gate)) {
    do_fade_out = true;
  }
  envelope_pitch_val = envelope_pitch_val_new;
  if (trigger_button_mute || envelope_pitch_val < ENVELOPE_PITCH_THRESHOLD) {
    do_fade_out = true;
  }

  // mutex
  sync_using_sdcard = true;

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

      printf(
          "phase[0] -> phase_new: %d*%d/%d -> %d\n", phases[0],
          banks[sel_bank_next]
              ->sample[sel_sample_next]
              .snd[sel_variation_next]
              ->size,
          banks[sel_bank_cur]->sample[sel_sample_cur].snd[sel_variation]->size,
          phase_new);
      printf("beat_current -> new beat_current: %d", beat_current);
      beat_current = round(((float)beat_current * (float)banks[sel_bank_next]
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

  // check if tempo matching is activated, if not then don't change
  // based on bpm
  uint32_t samples_to_read;
  if (banks[sel_bank_cur]
          ->sample[sel_sample_cur]
          .snd[sel_variation]
          ->tempo_match) {
    samples_to_read =
        round(buffer->max_sample_count * sf->bpm_tempo * envelope_pitch_val *
              pitch_vals[pitch_val_index]) *
        banks[sel_bank_cur]
            ->sample[sel_sample_cur]
            .snd[sel_variation]
            ->oversampling /
        banks[sel_bank_cur]->sample[sel_sample_cur].snd[sel_variation]->bpm;
  } else {
    samples_to_read = round((float)buffer->max_sample_count *
                            envelope_pitch_val * pitch_vals[pitch_val_index]) *
                      banks[sel_bank_cur]
                          ->sample[sel_sample_cur]
                          .snd[sel_variation]
                          ->oversampling;
  }

  uint32_t values_len = samples_to_read * banks[sel_bank_cur]
                                              ->sample[sel_sample_cur]
                                              .snd[sel_variation]
                                              ->num_channels;
  values_to_read = values_len * 2;  // 16-bit = 2 x 1 byte reads
  int16_t values[values_len];
  uint vol_main = (uint)round(sf->vol * retrig_vol * envelope_volume_val);

  if (!phase_change) {
    const int32_t next_phase =
        phases[0] + values_to_read * (phase_forward * 2 - 1);
    const int32_t splice_start = banks[sel_bank_cur]
                                     ->sample[sel_sample_cur]
                                     .snd[sel_variation]
                                     ->slice_start[banks[sel_bank_cur]
                                                       ->sample[sel_sample_cur]
                                                       .snd[sel_variation]
                                                       ->slice_current];
    const int32_t splice_stop = banks[sel_bank_cur]
                                    ->sample[sel_sample_cur]
                                    .snd[sel_variation]
                                    ->slice_stop[banks[sel_bank_cur]
                                                     ->sample[sel_sample_cur]
                                                     .snd[sel_variation]
                                                     ->slice_current];
    const int32_t sample_stop =
        banks[sel_bank_cur]->sample[sel_sample_cur].snd[sel_variation]->size;
    switch (banks[sel_bank_cur]
                ->sample[sel_sample_cur]
                .snd[sel_variation]
                ->play_mode) {
      case PLAY_NORMAL:
        if (phase_forward && phases[0] > sample_stop) {
          phase_change = true;
          phase_new = phases[0] - sample_stop;
        } else if (!phase_forward && phases[0] < 0) {
          phase_change = true;
          phase_new = phases[0] + sample_stop;
        }
        break;
      case PLAY_SPLICE_STOP:
        if ((phase_forward && (next_phase > splice_stop)) ||
            (!phase_forward && (next_phase < splice_start))) {
          do_fade_out = true;
        }
        break;
      case PLAY_SPLICE_LOOP:
        if (phase_forward && (phases[0] > splice_stop)) {
          phase_change = true;
          phase_new = splice_start;
        } else if (!phase_forward && (phases[0] < splice_stop)) {
          phase_change = true;
          phase_new = splice_stop;
        }
        break;
      case PLAY_SAMPLE_STOP:
        if ((phase_forward && (next_phase > sample_stop)) ||
            (!phase_forward && (next_phase < 0))) {
          do_fade_out = true;
        }
        break;
      case PLAY_SAMPLE_LOOP:
        if (phase_forward && (phases[0] > sample_stop)) {
          phase_change = true;
          phase_new = splice_start;
        } else if (!phase_forward && (phases[0] < 0)) {
          phase_change = true;
          phase_new = splice_stop;
        }
        break;
    }
  }

  if (phase_change) {
    do_crossfade = true;
    phases[1] = phases[0];  // old phase
    phases[0] = phase_new;
    phase_change = false;
  }

  if (audio_was_muted) {
    audio_was_muted = false;
    do_fade_in = true;
    // if fading in then do not crossfade
    do_crossfade = false;
  }
  // cpu_usage_flag is written when cpu usage is consistently high
  // in which case it will fade out audio and keep it muted for a little
  // bit to reduce cpu usage
  if (cpu_usage_flag == cpu_usage_flag_limit) {
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
      t0 = time_us_32();
      fr = f_close(&fil_current);
      if (fr != FR_OK) {
        debugf("[audio_callback] f_close error: %s\n", FRESULT_str(fr));
      }
      fr = f_open(
          &fil_current,
          banks[sel_bank_cur]->sample[sel_sample_cur].snd[sel_variation]->name,
          FA_READ);
      t1 = time_us_32();
      sd_card_total_time += (t1 - t0);

      if (fr != FR_OK) {
        debugf("[audio_callback] f_open error: %s\n", FRESULT_str(fr));
      }
    }

    // optimization here, only seek if the current position is not at the
    // phases[head]
    if (phases[head] != last_seeked) {
      t0 = time_us_32();
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
      t1 = time_us_32();
      sd_card_total_time += (t1 - t0);
    }

    t0 = time_us_32();
    if (f_read(&fil_current, values, values_to_read, &fil_bytes_read)) {
      printf("ERROR READING!\n");
      f_close(&fil_current);  // close and re-open trick
      f_open(
          &fil_current,
          banks[sel_bank_cur]->sample[sel_sample_cur].snd[sel_variation]->name,
          FA_READ);
      f_lseek(&fil_current, WAV_HEADER +
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
    sd_card_total_time += (t1 - t0);
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
    if (fx_active[FX_SATURATE]) {
      for (uint16_t i = 0; i < values_len; i++) {
        values[i] = transfer_doublesine(values[i]);
      }
    }

    // bitcrush before resampling
    if (fx_active[FX_BITCRUSH]) {
      for (uint16_t i = 0; i < values_len; i++) {
        if (i % 2 == 0) {
          // chop off bunch of bits
          values[0] = (values[0] >> 5) << 5;
        } else {
          // reduce sampling rate by half
          values[i] = values[i - 1];
        }
      }
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

  // apply other fx
  // TODO: fade in/out these fx using the crossfade?
  if (fx_active[FX_TREMELO] || fx_active[FX_PAN]) {
    int32_t u;
    int32_t v;
    int32_t w;
    if (fx_active[FX_TREMELO]) {
      u = q16_16_sin01(lfo_tremelo_val);
    }
    if (fx_active[FX_PAN]) {
      v = q16_16_sin01(lfo_pan_val);
      w = Q16_16_1 - v;
    }
    for (uint16_t i = 0; i < buffer->max_sample_count; i++) {
      for (uint8_t channel = 0; channel < 2; channel++) {
        if (fx_active[FX_TREMELO]) {
          samples[i * 2 + channel] =
              q16_16_multiply(samples[i * 2 + channel], u);
        }
        if (fx_active[FX_PAN]) {
          if (channel == 0) {
            samples[i * 2 + channel] =
                q16_16_multiply(samples[i * 2 + channel], v);
          } else {
            samples[i * 2 + channel] =
                q16_16_multiply(samples[i * 2 + channel], w);
          }
        }
      }
    }
  }

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
  t0 = time_us_32();
  give_audio_buffer(ap, buffer);
  give_audio_buffer_time = (time_us_32() - t0);

  if (do_fade_out) {
    printf("[audio_callback] do_fade_out -> audio_mute\n");
    audio_mute = true;
  }
  if (trigger_button_mute) {
    button_mute = true;
    trigger_button_mute = false;
  }

  sync_using_sdcard = false;

  clock_t endTime = time_us_64();
  cpu_utilizations[cpu_utilizations_i] =
      100 * (endTime - startTime) / (US_PER_BLOCK);
  cpu_utilizations_i++;

  if (cpu_utilizations_i == 64 || sd_card_total_time > 3000) {
    uint16_t cpu_utilization = 0;
    for (uint8_t i = 0; i < cpu_utilizations_i; i++) {
      cpu_utilization = cpu_utilization + cpu_utilizations[i];
    }
#ifdef PRINT_AUDIO_CPU_USAGE
    printf("average cpu utilization: %2.1f\n",
           ((float)cpu_utilization) / (float)cpu_utilizations_i);
#endif
    cpu_utilizations_i = 0;
#ifdef PRINT_SDCARD_TIMING
    printf("sdcard%2.1f %ld %d %d %ld\n", ((float)cpu_utilization) / 64.0,
           sd_card_total_time, values_to_read, give_audio_buffer_time,
           take_audio_buffer_time);
#endif
  }
  if (cpu_usage_flag == cpu_usage_flag_limit) {
    cpu_usage_flag = 0;
    reduce_cpu_usage = BLOCKS_PER_SECOND * 120 / sf->bpm_tempo;
  } else {
    if (cpu_utilizations[cpu_utilizations_i] > cpu_usage_limit_threshold) {
#ifdef PRINT_SDCARD_TIMING
      printf("sdcard%d %ld %d %d %ld\n", cpu_utilizations[cpu_utilizations_i],
             sd_card_total_time, values_to_read, give_audio_buffer_time,
             take_audio_buffer_time);
#endif
      cpu_usage_flag++;
      cpu_usage_flag_total++;
#ifdef PRINT_AUDIO_OVERLOADS
      if (cpu_usage_flag_total > 0) {
        clock_t currentTime = time_us_64();
        printf("cpu overloads every: %d ms\n",
               (currentTime - time_of_initialization) / 1000 /
                   cpu_usage_flag_total);
      }
#endif
      if (cpu_flag_counter == 0) {
        cpu_flag_counter = BLOCKS_PER_SECOND;
      }
      printf("cpu utilization: %d, flag: %d\n",
             cpu_utilizations[cpu_utilizations_i], cpu_usage_flag);
    } else {
      if (cpu_flag_counter > 0) {
        cpu_flag_counter--;
      } else {
        cpu_usage_flag = 0;
      }
    }
  }

  return;
}
