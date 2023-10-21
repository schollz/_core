
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

uint8_t cpu_utilizations[64];
uint8_t cpu_utilizations_i = 0;

void i2s_callback_func() {
  uint8_t sd_calls = 0;
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

  bool do_open_file = false;
  if (fil_is_open) {
    // check if the file is the right one
    if (fil_current_change) {
      fil_current_change = false;
      if (sel_bank_cur != sel_bank_next || sel_sample_cur != sel_sample_next) {
        int32_t phases0 = phases[0];
        phase_change = true;
        phase_new =
            phases[0] *
            banks[sel_bank_next]->sample[sel_sample_next].snd[0]->slice_num /
            banks[sel_bank_cur]->sample[sel_sample_cur].snd[0]->slice_num;
        // phase_new = (uint32)round(
        //     (float)phases[0] /
        //     (float)banks[sel_bank_cur]->sample[sel_sample_cur].snd[0]->size *
        //     (float)banks[sel_bank_next]->sample[sel_sample_next].snd[0]->size);
        // phase_new = (phase_new / PHASE_DIVISOR) * PHASE_DIVISOR;
        beat_current = round((float)beat_current *
                             (float)banks[sel_bank_next]
                                 ->sample[sel_sample_next]
                                 .snd[0]
                                 ->slice_num /
                             (float)banks[sel_bank_cur]
                                 ->sample[sel_sample_cur]
                                 .snd[0]
                                 ->slice_num);
        printf("next file: %s\n",
               banks[sel_bank_next]->sample[sel_sample_next].snd[0]->name);
        do_open_file = true;
        sel_sample_cur = sel_sample_next;
        sel_bank_cur = sel_bank_next;
      }
    }

    // flag for new phase
    if (phase_change) {
      phases[1] = phases[0];  // old phase
      phases[0] = (phase_new / PHASE_DIVISOR) * PHASE_DIVISOR;
      ResonantFilter_copy(resonantfilter[0], resonantfilter[1]);
    }

    envelope_pitch_val = Envelope2_update(envelope_pitch);
    // TODO: switch for if wobble is enabled
    // envelope_pitch_val =
    //     envelope_pitch_val * Range(LFNoise2(noise_wobble, 1), 0.9, 1.1);

    uint32_t samples_to_read =
        buffer->max_sample_count * round(sf->bpm_tempo * envelope_pitch_val) /
        banks[sel_bank_cur]->sample[sel_sample_cur].snd[0]->bpm;
    uint32_t values_len = samples_to_read * WAV_CHANNELS;
    uint32_t values_to_read = samples_to_read * WAV_CHANNELS * 2;
    int16_t values[values_len];
    uint vol_main =
        (uint)round(sf->vol * retrig_vol * Envelope2_update(envelope3));

    // TODO go from head 1 to head 0, in case there is a sd card change, so a
    // new sd file can be opened on head 0
    // phase_change = false;
    for (uint8_t head = 1; head >= 0; head--) {
      if (head == 1 && !phase_change) {
        continue;
      }
      if (head == 0 && do_open_file) {
        do_open_file = false;
        FRESULT fr;
        fr = f_close(&fil_current);  // close and re-open trick
        if (fr != FR_OK) {
          debugf("[audio_callback] f_close error: %s\n", FRESULT_str(fr));
        }
        fr = f_open(&fil_current,
                    banks[sel_bank_cur]->sample[sel_sample_cur].snd[0]->name,
                    FA_READ);
        if (fr != FR_OK) {
          debugf("[audio_callback] f_open error: %s\n", FRESULT_str(fr));
        }
      }

      if (f_lseek(&fil_current, WAV_HEADER + (phases[head] / PHASE_DIVISOR) *
                                                 PHASE_DIVISOR)) {
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

      ++sd_calls;
      if (f_read(&fil_current, values, values_to_read, &fil_bytes_read)) {
        printf("ERROR READING!\n");
        f_close(&fil_current);  // close and re-open trick
        f_open(&fil_current,
               banks[sel_bank_cur]->sample[sel_sample_cur].snd[0]->name,
               FA_READ);
        f_lseek(&fil_current,
                WAV_HEADER + (phases[head] / PHASE_DIVISOR) * PHASE_DIVISOR);
      }
      if (fil_bytes_read < values_to_read) {
        printf("asked for %d bytes, read %d bytes\n", values_to_read,
               fil_bytes_read);
      }

      if (!phase_forward) {
        // reverse audio
        for (int i = 0; i < values_len / 2; i++) {
          int16_t temp = values[i];
          values[i] = values[values_len - i - 1];
          values[values_len - i - 1] = temp;
        }
      }

#ifndef INCLUDE_STEREO
      int16_t *newArray = array_resample_linear(values, samples_to_read,
                                                buffer->max_sample_count);
      for (uint16_t i = 0; i < buffer->max_sample_count; i++) {
        newArray[i] = transfer_fn(newArray[i]);
#ifdef INCLUDE_FILTER
        if (filter_midi < 70) {
          // IIR_filter(myFilter0, &value0);
          newArray[i] =
              ResonantFilter_update(resonantfilter[head], newArray[i]);
        }
#endif
        if (head == 0) {
          samples[i * 2 + 0] = 0;
        }

        uint vol = vol_main;
        if (phase_change) {
          if (head == 0) {
            newArray[i] = crossfade3_in(newArray[i], i);
          } else {
            newArray[i] = crossfade3_out(newArray[i], i);
          }
        }
        int32_t value0 = (vol * newArray[i]) << 8u;
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

        newArrayL[i] = transfer_fn(newArrayL[i]);
        if (phase_change) {
          if (head == 0) {
            newArrayL[i] = (newArrayL[i] * (128 - crossfade2_raw[i]) / 128);
          } else {
            newArrayL[i] = (newArrayL[i] * crossfade2_raw[i] / 128);
          }
        }
        int32_t value0 = (vol * newArrayL[i]) << 8u;
#ifdef INCLUDE_FILTER
        if (filter_midi < 70) {
          IIR_filter(myFilter0, &value0);
        }
#endif
        newArrayR[i] = transfer_fn(newArrayR[i]);
        if (phase_change) {
          if (head == 0) {
            newArrayR[i] = (newArrayR[i] * (128 - crossfade2_raw[i]) / 128);
          } else {
            newArrayR[i] = (newArrayR[i] * crossfade2_raw[i] / 128);
          }
        }
        int32_t value1 = (vol * newArrayR[i]) << 8u;
#ifdef INCLUDE_FILTER
        if (filter_midi < 70) {
          IIR_filter(myFilter1, &value1);
        }
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
      if (phases[head] >=
          banks[sel_bank_cur]->sample[sel_sample_cur].snd[0]->size) {
        phases[head] -=
            banks[sel_bank_cur]->sample[sel_sample_cur].snd[0]->size;
      } else if (phases[head] < 0) {
        phases[head] +=
            banks[sel_bank_cur]->sample[sel_sample_cur].snd[0]->size;
      }
    }
  }
  sync_using_sdcard = false;
  phase_change = false;

  clock_t endTime = time_us_64();
  cpu_utilizations[cpu_utilizations_i] =
      100 * (endTime - startTime) / (US_PER_BLOCK);
  cpu_utilizations_i++;
  if (cpu_utilizations_i == 64) {
    uint16_t cpu_utilization = 0;
    for (uint8_t i = 0; i < 64; i++) {
      cpu_utilization = cpu_utilization + cpu_utilizations[i];
    }
    printf("average cpu utilization: %2.1f\n", sd_calls,
           ((float)cpu_utilization) / 64.0);
    cpu_utilizations_i = 0;
  }

  return;
}
