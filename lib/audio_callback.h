
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
        int32_t phases0 = phases[0];
        phase_change = true;
        phase_new =
            phases[0] *
            file_list[fil_current_bank_next].beats[fil_current_id_next] /
            file_list[fil_current_bank].beats[fil_current_id];
        beat_current = round(
            (float)beat_current *
            (float)file_list[fil_current_bank_next].beats[fil_current_id_next] /
            (float)file_list[fil_current_bank].beats[fil_current_id]);
        printf("[current: %d/%d, next: %d/%d]\n", phases0,
               file_list[fil_current_bank].size[fil_current_id], phase_new,
               file_list[fil_current_bank_next].size[fil_current_id_next]);
        FRESULT fr;
        fr = f_close(&fil_current);  // close and re-open trick
        if (fr != FR_OK) {
          debugf("error: %d\n", fr);
        }
        fr = f_open(&fil_current,
                    file_list[fil_current_bank_next].name[fil_current_id_next],
                    FA_READ);
        if (fr != FR_OK) {
          debugf("error: %d\n", fr);
        }
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
