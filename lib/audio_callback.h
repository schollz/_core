// Copyright 2023-2025 Zack Scholl, GPLv3.0

typedef struct {
  uint32_t a;  // alpha * 65536
  uint32_t b;  // (1 - alpha) * 65536
} onepole_coeff_t;

static const onepole_coeff_t onepole_table[20] = {
    // fc = 100 Hz
    {64622, 914},
    // fc = 200 Hz
    {63733, 1803},
    // fc = 300 Hz
    {62856, 2680},
    // fc = 500 Hz
    {61185, 4351},
    // fc = 700 Hz
    {59594, 5942},
    // fc = 1000 Hz
    {57344, 8192},
    // fc = 1500 Hz
    {54316, 11220},
    // fc = 2000 Hz
    {51703, 13833},
    // fc = 3000 Hz
    {47666, 17870},
    // fc = 4000 Hz
    {44301, 21235},
    // fc = 5000 Hz
    {41453, 24083},
    // fc = 7000 Hz
    {36864, 28672},
    // fc = 8000 Hz
    {34953, 30583},
    // fc = 10000 Hz
    {27027, 39329},  // <-- your original value
    // fc = 12000 Hz
    {24289, 42047},
    // fc = 15000 Hz
    {21037, 45399},
    // fc = 18000 Hz
    {18309, 48127},
    // fc = 20000 Hz
    {16804, 49632},
    // fc = 22050 Hz (Nyquist)
    {16069, 50367},
    // fc = 24000 Hz (slightly above Nyquist, still stable)
    {15322, 51114}};

uint8_t cpu_utilizations[64];
uint8_t cpu_utilizations_i = 0;
uint32_t last_seeked = 1;
uint32_t reduce_cpu_usage = 0;
uint32_t cpu_usage_flag_total = 0;
uint8_t cpu_usage_flag = 0;
uint16_t cpu_flag_counter = 0;
const uint8_t cpu_usage_flag_limit = 60;
const uint8_t cpu_usage_limit_threshold = 150;

bool audio_was_muted = false;
bool do_open_file_ready = false;
bool muted_because_of_sel_variation = false;
bool first_loop_ever = true;
int32_t reverb_fade = 0;
int32_t amiga_previous_value[2];
bool reverb_activated = false;
bool mute_soft_activated = false;

inline int32_t scale16to32_fixed_dither(int16_t val) {
  return (((int32_t)val) << 16);  // + ((rand() & 1) - 1);
}

void __not_in_flash_func(update_filter_from_envelope)(int32_t val) {
  for (uint8_t channel = 0; channel < 2; channel++) {
    ResonantFilter_setFilterType(resFilter[channel], global_filter_lphp);
    ResonantFilter_setFc(resFilter[channel], val);
  }
}

#define INTERPOLATE_VALUE 512
int16_t newArray[SAMPLES_PER_BUFFER];

void __not_in_flash_func(array_resample_linear441)(int16_t *arr,
                                                   int16_t arr_size) {
  // If the sizes match, simply copy the input array
  if (arr_size == SAMPLES_PER_BUFFER) {
    for (int16_t i = 0; i < SAMPLES_PER_BUFFER; i++) {
      newArray[i] = arr[i];
    }
  }

  // Calculate step size in fixed-point format
  uint32_t stepSize = (arr_size)*INTERPOLATE_VALUE / (SAMPLES_PER_BUFFER);

  for (int16_t i = 0; i < SAMPLES_PER_BUFFER; i++) {
    uint32_t indexFixed = i * stepSize;               // Fixed-point index
    uint32_t index = indexFixed / INTERPOLATE_VALUE;  // Integer part
    uint32_t frac = indexFixed % INTERPOLATE_VALUE;   // Fractional part

    // Perform fixed-point linear interpolation
    int32_t x = ((int32_t)arr[index] * (INTERPOLATE_VALUE - frac)) +
                ((int32_t)arr[index + 1] * frac);
    newArray[i] = x / INTERPOLATE_VALUE;
  }
}

#ifdef DEBUG_AUDIO_WITH_SINE_WAVE
uint32_t sine_wave_counter = 0;
#endif

void __not_in_flash_func(i2s_callback_func)() {
  // void i2s_callback_func() {
  uint32_t t0, t1;
  uint32_t sd_card_total_time = 0;
#ifdef PRINT_SDCARD_TIMING
  uint32_t give_audio_buffer_time = 0;
  uint32_t take_audio_buffer_time = 0;
#endif

  // flag for new phase
  bool do_crossfade = false;
  bool do_fade_out = false;
  bool do_fade_in = false;
  clock_t startTime = time_us_64();
  audio_buffer_t *buffer = take_audio_buffer(ap, false);
#ifdef PRINT_SDCARD_TIMING
  take_audio_buffer_time = (time_us_64() - startTime);
#endif
  if (buffer == NULL) {
    return;
  }

  int32_t samples[buffer->max_sample_count * 2];
  int16_t *samples16 = (int16_t *)buffer->buffer->bytes;

#ifdef DEBUG_AUDIO_WITH_SINE_WAVE
  for (uint16_t i = 0; i < buffer->max_sample_count; i++) {
    int32_t value0 =
        (int32_t)(sinf(2 * M_PI * 440 * sine_wave_counter / 44100) *
                  (0x7ffffff0));
    sine_wave_counter++;
    samples[i * 2 + 0] = value0;
    samples[i * 2 + 1] = value0;
  }
  buffer->sample_count = buffer->max_sample_count;
  for (uint16_t i = 0; i < buffer->max_sample_count; i++) {
    samples16[i * 2 + 0] = (int16_t)(samples[i * 2 + 0] >> 16);
    samples16[i * 2 + 1] = (int16_t)(samples[i * 2 + 1] >> 16);
  }
  give_audio_buffer(ap, buffer);
  return;
#endif

  EnvelopeLinearInteger_update(envelope_filter, update_filter_from_envelope);

  float envelope_volume_val = Envelope2_update(envelope_volume);
  float envelope_pitch_val_new = Envelope2_update(envelope_pitch);

  if (mute_because_of_playback_type || sync_using_sdcard || !fil_is_open ||
      button_mute || reduce_cpu_usage > 0 ||
      (envelope_pitch_val < ENVELOPE_PITCH_THRESHOLD) ||
      envelope_volume_val < 0.001 || Gate_is_up(audio_gate) ||
      (clock_in_do &&
       ((startTime - clock_in_last_time) > clock_in_diff_2x &&
        clock_start_stop_sync) &&
       !(use_onewiremidi || usb_midi_present))) {
    first_loop_ever = true;
    audio_was_muted = true;
    audio_callback_in_mute = true;

    envelope_pitch_val = envelope_pitch_val_new;

    if (muted_because_of_sel_variation) {
      if (sel_variation == sel_variation_next) {
        muted_because_of_sel_variation = false;
        goto BREAKOUT_OF_MUTE;
      }
    }

    // continue to update the gate
    Gate_update(audio_gate, sf->bpm_tempo);

    // check cpu usage
    if (reduce_cpu_usage > 0) {
      // printf("reduce_cpu_usage: %d\n", reduce_cpu_usage);
      reduce_cpu_usage--;
    }
    int16_t values[buffer->max_sample_count];
    for (uint16_t i = 0; i < buffer->max_sample_count; i++) {
      values[i] = 0;
    }

    // saturate before resampling?
    if (sf->fx_active[FX_SATURATE]) {
      Saturation_process(saturation, values, buffer->max_sample_count);
    }

    // // fuzz
    // if (sf->fx_active[FX_FUZZ]) {
    //   Fuzz_process(values, buffer->max_sample_count);
    // }

    // // bitcrush
    // if (sf->fx_active[FX_BITCRUSH]) {
    //   Bitcrush_process(values, buffer->max_sample_count);
    // }
    int32_t vol_main =
        round((float)volume_vals[sf->vol] * retrig_vol * envelope_volume_val);
    for (uint16_t i = 0; i < buffer->max_sample_count; i++) {
      samples[i * 2 + 0] =
          q16_16_multiply(((int32_t)values[i]) << 16, vol_main);
      samples[i * 2 + 1] = samples[i * 2 + 0];  // R = L
    }
    buffer->sample_count = buffer->max_sample_count;

#ifdef INCLUDE_SINEBASS
    if (fil_is_open) {
      // apply bass
      for (uint16_t i = 0; i < buffer->max_sample_count; i++) {
        int32_t v = WaveBass_next(wavebass);
        samples[i * 2 + 0] += v;
        samples[i * 2 + 1] += v;
      }
    }
#endif

    // apply reverb
    if (sf->fx_active[FX_EXPAND]) {
      if (freeverb != NULL) {
        FV_Reverb_process(freeverb, samples, buffer->max_sample_count);
      }
    } else {
      // apply delay
      Delay_process(delay, samples, buffer->max_sample_count, 0);
    }

    for (uint16_t i = 0; i < buffer->max_sample_count; i++) {
      samples16[i * 2 + 0] = (int16_t)(samples[i * 2 + 0] >> 16);
      samples16[i * 2 + 1] = (int16_t)(samples[i * 2 + 1] >> 16);
    }
    give_audio_buffer(ap, buffer);

    // audio muted flag to ensure a fade in occurs when
    // unmuted
    return;
  }

BREAKOUT_OF_MUTE:
  audio_callback_in_mute = false;

  if (playback_restarted) {
    // printf("[audio_callback] playback_restarted\n");
    playback_restarted = false;
    audio_was_muted = false;
  }

  Gate_update(audio_gate, sf->bpm_tempo);
  envelope_pitch_val = envelope_pitch_val_new;

  if (trigger_button_mute || envelope_pitch_val < ENVELOPE_PITCH_THRESHOLD ||
      Gate_is_up(audio_gate) || sel_variation != sel_variation_next) {
    muted_because_of_sel_variation = sel_variation != sel_variation_next;
    // printf("[audio_callback] muted_because_of_sel_variation: %d\n",
    //        muted_because_of_sel_variation);
    // printf("[audio_callback] trigger_button_mute: %d\n",
    // trigger_button_mute);
    do_fade_out = true;
  }

  // mutex
  sync_using_sdcard = true;

  bool do_open_file = do_open_file_ready;
  // check if the file is the right one
  if (do_open_file_ready) {
    // printf("[audio_callback] next file: %s\n", banks[sel_bank_next]
    //                               ->sample[sel_sample_next]
    //                               .snd[sel_variation_next]
    //                               ->name);
    phases[0] = round(
        ((float)phases[0] *
         (float)banks[sel_bank_next]
             ->sample[sel_sample_next]
             .snd[FILEZERO]
             ->size *
         sel_variation_scale[sel_variation]) /
        (float)banks[sel_bank_cur]->sample[sel_sample_cur].snd[FILEZERO]->size *
        sel_variation_scale[sel_variation]);

    // printf("[audio_callback] phase[0] -> phase_new: %d*%d/%d -> %d\n",
    // phases[0],
    //        banks[sel_bank_next]
    //            ->sample[sel_sample_next]
    //            .snd[sel_variation_next]
    //            ->size,
    //        banks[sel_bank_cur]->sample[sel_sample_cur].snd[FILEZERO]->size,
    //        phase_new);
    // printf("[audio_callback] beat_current -> new beat_current: %d",
    // beat_current);
    beat_current = round(((float)beat_current * (float)banks[sel_bank_next]
                                                    ->sample[sel_sample_next]
                                                    .snd[FILEZERO]
                                                    ->slice_num)) /
                   (float)banks[sel_bank_cur]
                       ->sample[sel_sample_cur]
                       .snd[FILEZERO]
                       ->slice_num;
    // printf(" -> %d\n", beat_current);
    do_open_file = true;
    do_fade_in = true;
    do_open_file_ready = false;
    // printf("[audio_callback] do_fade_in from do_open_file_ready\n");
  }
  if (fil_current_change || fil_current_change_force) {
    fil_current_change = false;
    if (fil_current_change_force || sel_bank_cur != sel_bank_next ||
        sel_sample_cur != sel_sample_next) {
      do_open_file_ready = true;
      do_fade_out = true;
      fil_current_change_force = false;
      // printf("[audio_callback] do_fade_out, readying do_open_file_ready\n");
    }
  }

  // check if scratch is active
  float scratch_pitch = 1.0;
  bool change_phase_forward = false;
  if (sf->fx_active[FX_SCRATCH] && sf->fx_param[FX_SCRATCH][0] > 20) {
    scratch_lfo_val += scratch_lfo_inc;
    scratch_pitch = q16_16_fp_to_float(q16_16_cos(scratch_lfo_val));
    if (scratch_pitch < 0) {
      scratch_pitch = -scratch_pitch;
      change_phase_forward = true;
      phase_forward = !phase_forward;
    }
    if (scratch_pitch < 0.05) {
      scratch_pitch = 0.05;
    }
  }

  // check if tempo matching is activated, if not then don't change
  // based on bpm
  uint32_t samples_to_read;
  if (banks[sel_bank_cur]->sample[sel_sample_cur].snd[FILEZERO]->tempo_match) {
    samples_to_read =
        round(buffer->max_sample_count * sf->bpm_tempo * envelope_pitch_val *
              pitch_vals[sf->pitch_val_index] * scratch_pitch *
              pitch_vals[retrig_pitch] *
              (banks[sel_bank_cur]
                   ->sample[sel_sample_cur]
                   .snd[FILEZERO]
                   ->oversampling +
               1) /
              banks[sel_bank_cur]->sample[sel_sample_cur].snd[FILEZERO]->bpm);
  } else {
    samples_to_read =
        round((float)buffer->max_sample_count * envelope_pitch_val *
              pitch_vals[sf->pitch_val_index] * scratch_pitch *
              pitch_vals[retrig_pitch]) *
        (banks[sel_bank_cur]
             ->sample[sel_sample_cur]
             .snd[FILEZERO]
             ->oversampling +
         1);
  }
  if (samples_to_read < 11) {
    samples_to_read = 11;
  }

  uint32_t values_len =
      (samples_to_read + 1) *
      (banks[sel_bank_cur]->sample[sel_sample_cur].snd[FILEZERO]->num_channels +
       1);
  uint32_t values_len_minus_peek =
      (samples_to_read) *
      (banks[sel_bank_cur]->sample[sel_sample_cur].snd[FILEZERO]->num_channels +
       1);
  uint32_t values_to_read_minus_peek = values_len_minus_peek * 2;
  uint32_t values_to_read = values_len * 2;  // 16-bit = 2 x 1 byte reads
  int16_t values[values_len];
  int32_t vol_main =
      round((float)volume_vals[sf->vol] * retrig_vol * envelope_volume_val);

  if (!phase_change) {
    const int32_t next_phase = phases[0] + ((samples_to_read) *
                                            (banks[sel_bank_cur]
                                                 ->sample[sel_sample_cur]
                                                 .snd[FILEZERO]
                                                 ->num_channels +
                                             1) *
                                            2) *
                                               (phase_forward * 2 - 1);
    const int32_t splice_start = banks[sel_bank_cur]
                                     ->sample[sel_sample_cur]
                                     .snd[FILEZERO]
                                     ->slice_start[banks[sel_bank_cur]
                                                       ->sample[sel_sample_cur]
                                                       .snd[FILEZERO]
                                                       ->slice_current];
    const int32_t splice_stop = banks[sel_bank_cur]
                                    ->sample[sel_sample_cur]
                                    .snd[FILEZERO]
                                    ->slice_stop[banks[sel_bank_cur]
                                                     ->sample[sel_sample_cur]
                                                     .snd[FILEZERO]
                                                     ->slice_current];
    const int32_t sample_stop =
        banks[sel_bank_cur]->sample[sel_sample_cur].snd[FILEZERO]->size *
        sel_variation_scale[sel_variation];

    switch (
        banks[sel_bank_cur]->sample[sel_sample_cur].snd[FILEZERO]->play_mode) {
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
          mute_because_of_playback_type = true;
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
          mute_because_of_playback_type = true;
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
    // printf("[audio_callback] audio_was_muted, fading in\n");
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
    if (head == 1 && (!do_crossfade || do_fade_in)) {
      continue;
    }

    if (head == 0 && do_open_file) {
      // setup the next
      sel_bank_cur = sel_bank_next;
      sel_sample_cur = sel_sample_next % banks[sel_bank_cur]->num_samples;
      // printf("[audio_callback] switch bank/sample %d/%d\n", sel_bank_cur,
      //        sel_sample_cur);

      FRESULT fr;
      t0 = time_us_32();
      fr = f_close(&fil_current);
      if (fr != FR_OK) {
        debugf("[audio_callback] f_close error: %s\n", FRESULT_str(fr));
      }
      sprintf(fil_current_name, "bank%d/%d.%d.wav", sel_bank_cur + 1,
              sel_sample_cur, sel_variation + audio_variant * 2);
      fr = f_open(&fil_current, fil_current_name, FA_READ);
      t1 = time_us_32();
      sd_card_total_time += (t1 - t0);
#ifdef PRINT_SDCARD_OPEN_TIMING
      if (do_open_file) {
        MessageSync_printf(messagesync,
                           "[audio_callback] do_open_file f_close+f_open: %d\n",
                           (t1 - t0));
      }
#endif
      if (fr != FR_OK) {
        debugf("[audio_callback] f_open error: %s\n", FRESULT_str(fr));
      }
    }

    // optimization here, only seek if the current position is not at the
    // phases[head]
    if (phases[head] != last_seeked || do_open_file) {
      t0 = time_us_32();
      int negative_latency = 0;
#ifdef INCLUDE_ECTOCORE
      if (latency_factor > 0) {
        negative_latency =
            roundf((float)values_len_minus_peek * latency_factor) *
            (phase_forward * 2 - 1);
        if (clock_input_present_first) {
          clock_input_present_first = false;
          negative_latency = 0;
          MessageSync_printf(messagesync,
                             "[audio_callback] clock_input_present_first\n");
        }
      }
#endif
      if (f_lseek(&fil_current,
                  WAV_HEADER +
                      ((banks[sel_bank_cur]
                            ->sample[sel_sample_cur]
                            .snd[FILEZERO]
                            ->num_channels +
                        1) *
                       (banks[sel_bank_cur]
                            ->sample[sel_sample_cur]
                            .snd[FILEZERO]
                            ->oversampling +
                        1) *
                       44100) +
                      ((phases[head] + negative_latency) / PHASE_DIVISOR) *
                          PHASE_DIVISOR) != FR_OK) {
        printf("problem seeking to phase (%d)\n", phases[head]);
        for (uint16_t i = 0; i < buffer->max_sample_count; i++) {
          int32_t value0 = 0;
          samples[i * 2 + 0] = value0 + (value0 >> 16u);  // L
          samples[i * 2 + 1] = samples[i * 2 + 0];        // R = L
        }
        buffer->sample_count = buffer->max_sample_count;
        for (uint16_t i = 0; i < buffer->max_sample_count; i++) {
          samples16[i * 2 + 0] = (int16_t)(samples[i * 2 + 0] >> 16);
          samples16[i * 2 + 1] = (int16_t)(samples[i * 2 + 1] >> 16);
        }
        give_audio_buffer(ap, buffer);
        sync_using_sdcard = false;
        // sdcard_startup();
        return;
      }
      t1 = time_us_32();
#ifdef PRINT_SDCARD_OPEN_TIMING
      if (do_open_file) {
        MessageSync_printf(messagesync,
                           "[audio_callback] do_open_file f_lseek: %d\n",
                           (t1 - t0));
      }
#endif
      sd_card_total_time += (t1 - t0);
    }

    t0 = time_us_32();
    if (f_read(&fil_current, values, values_to_read, &fil_bytes_read)) {
      printf("ERROR READING!\n");
      watchdog_reboot(0, SRAM_END, 0);
      // sprintf(fil_current_name, "bank%d/%d.%d.wav", sel_bank_cur + 1,
      //         sel_sample_cur, sel_variation + audio_variant * 2);
      // printf("reopening %s\n", fil_current_name);
      // f_close(&fil_current);  // close and re-open trick
      // f_open(&fil_current, fil_current_name, FA_READ);
      // f_lseek(&fil_current, WAV_HEADER +
      //                           ((banks[sel_bank_cur]
      //                                 ->sample[sel_sample_cur]
      //                                 .snd[FILEZERO]
      //                                 ->num_channels +
      //                             1) *
      //                            (banks[sel_bank_cur]
      //                                 ->sample[sel_sample_cur]
      //                                 .snd[FILEZERO]
      //                                 ->oversampling +
      //                             1) *
      //                            44100) +
      //                           (phases[head] / PHASE_DIVISOR) *
      //                           PHASE_DIVISOR);
    }
    t1 = time_us_32();
    sd_card_total_time += (t1 - t0);
#ifdef PRINT_SDCARD_TIMING
    if (do_open_file) {
      MessageSync_printf(
          messagesync, "[audio_callback] do_open_file f_read: %d\n", (t1 - t0));
    }
#endif
    last_seeked = phases[head] + fil_bytes_read;

    if (fil_bytes_read < values_to_read) {
      MessageSync_printf(messagesync,
                         "%d %d: asked for %d bytes, read %d bytes\n",
                         phases[head],
                         WAV_HEADER +
                             ((banks[sel_bank_cur]
                                   ->sample[sel_sample_cur]
                                   .snd[FILEZERO]
                                   ->num_channels +
                               1) *
                              (banks[sel_bank_cur]
                                   ->sample[sel_sample_cur]
                                   .snd[FILEZERO]
                                   ->oversampling +
                               1) *
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

    // beat repeat
    BeatRepeat_process(beatrepeat, values, values_len);

    // saturate before resampling?
    if (sf->fx_active[FX_SATURATE]) {
      for (uint16_t i = 0; i < values_len; i++) {
        values[i] = values[i] * sf->fx_param[FX_SATURATE][0] / 128;
      }
      if (audio_variant_num > 0) {
        set_audio_variant(sf->fx_param[FX_SATURATE][1] * audio_variant_num /
                          256);
      }
      Saturation_process(saturation, values, values_len);
    }

    // shaper
    if (sf->fx_active[FX_SHAPER]) {
      if (sf->fx_param[FX_SHAPER][0] > 128) {
        Shaper_expandUnder_compressOver_process(
            values, values_len, (sf->fx_param[FX_SHAPER][0] - 128) << 6,
            sf->fx_param[FX_SHAPER][1]);
      } else {
        Shaper_expandOver_compressUnder_process(values, values_len,
                                                sf->fx_param[FX_SHAPER][0] << 6,
                                                sf->fx_param[FX_SHAPER][1]);
      }
    }

    if (sf->fx_active[FX_FUZZ]) {
      Fuzz_process(values, values_len, sf->fx_param[FX_FUZZ][0],
                   sf->fx_param[FX_FUZZ][1]);
    }

    // bitcrush
    if (sf->fx_active[FX_BITCRUSH]) {
      Bitcrush_process(values, values_len, sf->fx_param[FX_BITCRUSH][0],
                       sf->fx_param[FX_BITCRUSH][1]);
    }

    if (banks[sel_bank_cur]
            ->sample[sel_sample_cur]
            .snd[FILEZERO]
            ->num_channels == 0) {
      // mono
      array_resample_linear441(values, samples_to_read);

      for (uint16_t i = 0; i < buffer->max_sample_count; i++) {
        if (do_crossfade && !do_fade_in) {
          if (head == 0) {
            newArray[i] = crossfade3_in(newArray[i], i, CROSSFADE3_COS);
          } else {
            newArray[i] = crossfade3_out(newArray[i], i, CROSSFADE3_COS);
          }
        } else if (do_fade_out) {
          newArray[i] = crossfade3_out(newArray[i], i, CROSSFADE3_COS);
        } else if (do_fade_in) {
          newArray[i] = crossfade3_in(newArray[i], i, CROSSFADE3_COS);
        }

        if (first_loop) {
          samples[i * 2 + 0] =
              q16_16_multiply(((int32_t)newArray[i]) << 16, vol_main);
          if (head == 0) {
            samples[i * 2 + 1] = samples[i * 2 + 0];  // R = L
          }
        } else {
          samples[i * 2 + 0] +=
              q16_16_multiply(((int32_t)newArray[i]) << 16, vol_main);
          samples[i * 2 + 1] = samples[i * 2 + 0];  // R = L
        }
        // int32_t value0 = (vol * newArray[i]) << 8u;
        // samples[i * 2 + 0] =
        //     samples[i * 2 + 0] + value0 + (value0 >> 16u);  // L
      }
      if (first_loop) {
        first_loop = false;
      }
    } else if (banks[sel_bank_cur]
                   ->sample[sel_sample_cur]
                   .snd[FILEZERO]
                   ->num_channels == 1) {
      // stereo
      for (uint8_t channel = 0; channel < 2; channel++) {
        int16_t valuesC[values_len / 2];  // max limit
        for (uint16_t i = 0; i < values_len; i++) {
          if (i % 2 == channel) {
            valuesC[i / 2] = values[i];
          }
        }

        array_resample_linear441(valuesC, samples_to_read);

        // TODO: function pointer for audio block here?
        for (uint16_t i = 0; i < buffer->max_sample_count; i++) {
          if (first_loop) {
            samples[i * 2 + channel] = 0;
          }

          if (do_crossfade && !do_fade_in) {
            if (head == 0) {
              newArray[i] = crossfade3_in(newArray[i], i, CROSSFADE3_COS);
            } else {
              newArray[i] = crossfade3_out(newArray[i], i, CROSSFADE3_COS);
            }
          } else if (do_fade_out) {
            newArray[i] = crossfade3_out(newArray[i], i, CROSSFADE3_COS);
          } else if (do_fade_in) {
            newArray[i] = crossfade3_in(newArray[i], i, CROSSFADE3_COS);
          }
          samples[i * 2 + channel] +=
              q16_16_multiply(((int32_t)newArray[i]) << 16, vol_main);
        }
      }
      first_loop = false;
    }
    phases[head] += (values_to_read_minus_peek * (phase_forward * 2 - 1));
  }

#ifdef INCLUDE_ECTOCORE
  if (mute_soft) {
    for (uint16_t i = 0; i < buffer->max_sample_count; i++) {
      samples[i * 2 + 0] = 0;
      samples[i * 2 + 1] = 0;
    }
    mute_soft_activated = true;
  } else if (mute_soft_activated) {
    // fade in
    for (uint16_t i = 0; i < buffer->max_sample_count; i++) {
      samples[i * 2 + 0] =
          q16_16_multiply(samples[i * 2 + 0], crossfade3_cos_in[i]);
      samples[i * 2 + 1] =
          q16_16_multiply(samples[i * 2 + 1], crossfade3_cos_in[i]);
    }
    mute_soft_activated = false;
  }
#endif

#ifdef INCLUDE_CUEDSOUNDS
#ifdef INCLUDE_ECTOCORE
  bool cuedsounds_played =
      cuedsounds_audio_update(samples, buffer->max_sample_count, vol_main);
  if (cuedsounds_played) {
    mute_soft = false;
  }
#endif
#ifdef INCLUDE_ZEPTOCORE
  cuedsounds_audio_update(samples, buffer->max_sample_count, vol_main);
#endif
#endif

// apply filter
#ifdef INCLUDE_FILTER
  for (uint8_t channel = 0; channel < 2; channel++) {
    ResonantFilter_update(resFilter[channel], samples, buffer->max_sample_count,
                          channel);
  }
#endif

  // apply other fx
  // TODO: fade in/out these fx using the crossfade?
  // TODO: LFO's move to main thread?
  if (sf->fx_active[FX_PAN]) {
    // int32_t u;
    int32_t v;
    int32_t w;
    // if (sf->fx_active[FX_TREMELO]) {
    //   uint8_t vv = linlin(sf->fx_param[FX_TREMELO][1], 0, 255, 128, 255);
    //   u = q16_16_sin01(lfo_tremelo_val);
    //   u = u * vv / 255 + (Q16_16_1 * (255 - vv) / 255);
    // }
    if (sf->fx_active[FX_PAN]) {
      uint8_t vv = linlin(sf->fx_param[FX_PAN][1], 0, 255, 128, 255);
      v = q16_16_sin01(lfo_pan_val);
      v = v * vv / 255 + (Q16_16_1 * (255 - vv) / 255);
      w = Q16_16_1 - v;
    }
    for (uint16_t i = 0; i < buffer->max_sample_count; i++) {
      for (uint8_t channel = 0; channel < 2; channel++) {
        // if (sf->fx_active[FX_TREMELO]) {
        //   samples[i * 2 + channel] =
        //       q16_16_multiply(samples[i * 2 + channel], u);
        // }
        if (sf->fx_active[FX_PAN]) {
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

  // apply reverb
  if (sf->fx_active[FX_EXPAND] || reverb_fade > 0 || reverb_activated) {
    if (freeverb != NULL) {
      if (first_loop_ever) {
        // time this process
        t0 = time_us_32();
      }
      if (reverb_activated && !sf->fx_active[FX_EXPAND]) {
        reverb_activated = false;
        if (reverb_fade <= 0) {
          reverb_fade = Q16_16_0_85;
        }
      }
      if (!reverb_activated && sf->fx_active[FX_EXPAND]) {
        reverb_activated = true;
        if (reverb_fade <= 0) {
          reverb_fade = Q16_16_0_85;
        }
      }
      if (reverb_fade > 0) {
        // MessageSync_printf(messagesync, "%d fade: %ld\n", reverb_activated,
        //                    reverb_fade);
        reverb_fade -= 300;
        if (reverb_fade < 0) {
          reverb_fade = 0;
        }
        if (sf->fx_active[FX_EXPAND]) {
          // fade in
          FV_Reverb_set_wet(freeverb, Q16_16_0_85 - reverb_fade);
        } else {
          // fade out
          FV_Reverb_set_wet(freeverb, reverb_fade);
        }
      }
      FV_Reverb_process(freeverb, samples, buffer->max_sample_count);

      if (first_loop_ever) {
        // MessageSync_printf(messagesync, "freeverb : %ld us\n",
        //                    (time_us_32() - t0));
        first_loop_ever = false;
      }
    }
  } else {
#ifdef INCLUDE_ECTOCORE
    if (sf->fx_active[FX_DELAY]) {
      Delay_setFeedbackf(delay,
                         Range(LFNoise2_period(noise_feedback, 1), 0.49, 0.99));
      float v = Range(LFNoise2_period(noise_duration, 2), 100, 10000);
      Delay_setDuration(delay, v);
      // float v = Range(LFNoise2_period(noise_duration, 2), 6.64f, 13.28f);
      // Delay_setDuration(delay, powf(2, v));
    }
    Delay_process(delay, samples, buffer->max_sample_count, 0);
#else
    if (sf->fx_active[FX_DELAY] && sf->fx_param[FX_DELAY][2] > 30) {
      Delay_setFeedbackf(delay,
                         Range(LFNoise2_period(noise_feedback, 2), 0.49, 0.99));
      float v = Range(LFNoise2_period(noise_duration, 2), 100, 10000);
      Delay_setDuration(delay, v);
      // float v = Range(LFNoise2_period(noise_duration, 2), 6.64f, 13.28f);
      // // raise v from the 10th power
      // Delay_setDuration(delay, powf(2, v));
    }
    Delay_process(delay, samples, buffer->max_sample_count, 0);
#endif
  }

  // apply comb
  Comb_process(combfilter, samples, buffer->max_sample_count);

#ifdef INCLUDE_SINEBASS
  // apply bass
  for (uint16_t i = 0; i < buffer->max_sample_count; i++) {
    int32_t v = WaveBass_next(wavebass);
    samples[i * 2 + 0] += v;
    samples[i * 2 + 1] += v;
  }
#endif

#ifndef INCLUDE_ECTOCORE
  if (clock_out_do) {
    if (clock_out_ready) {
      clock_out_ready = false;
      for (uint16_t i = 0; i < buffer->max_sample_count; i++) {
        samples[i * 2 + 0] = 2147483645;
      }
    } else {
      for (uint16_t i = 0; i < buffer->max_sample_count; i++) {
        samples[i * 2 + 0] = 0;
      }
    }
  }
#endif

#ifdef INCLUDE_ECTOCORE
  for (uint16_t i = 0; i < buffer->max_sample_count; i++) {
    samples[i * 2 + 0] *= -1;
    samples[i * 2 + 1] *= -1;
  }
#endif

  if (mode_amiga_filter_index < 20) {
    // simple filter
    int32_t v[2];
    for (uint16_t i = 0; i < buffer->max_sample_count; i++) {
      for (uint8_t j = 0; j < 2; j++) {
        // one-pole filter converted to fixed point
        // static const float RC = 1.0 / (2.0 * 3.14159265359 * CUTOFF_FREQ);
        // static const float alpha = RC / (RC + (1.0 / SAMPLE_RATE));
        // float output = alpha * (*prev_output) + (1.0 - alpha) * input;
        // 1 = 65536
        // filtering at 10khz
        v[j] = samples[i * 2 + j];
        // add a random value (-1 to 1)
        // v[j] += (rand() % 3) - 1;
        // v[j] = q16_16_multiply(27027, amiga_previous_value[j]) +
        //        q16_16_multiply(39329, v[j]);
        v[j] = q16_16_multiply(onepole_table[mode_amiga_filter_index].a,
                               amiga_previous_value[j]) +
               q16_16_multiply(onepole_table[mode_amiga_filter_index].b, v[j]);

        amiga_previous_value[j] = v[j];
      }

      if (i % 2 == 0) {
        // convert 32-bit to 8-bit
        samples[i * 2 + 0] = (int32_t)((int8_t)(v[0] >> 24)) << 24;
        samples[i * 2 + 1] = (int32_t)((int8_t)(v[1] >> 24)) << 24;
      } else {
        samples[i * 2 + 0] = samples[i * 2 - 2];
        samples[i * 2 + 1] = samples[i * 2 - 1];
      }
    }
  }

  buffer->sample_count = buffer->max_sample_count;
  t0 = time_us_32();
  for (uint16_t i = 0; i < buffer->max_sample_count; i++) {
    samples16[i * 2 + 0] = (int16_t)(samples[i * 2 + 0] >> 16);
    samples16[i * 2 + 1] = (int16_t)(samples[i * 2 + 1] >> 16);
  }
  give_audio_buffer(ap, buffer);
#ifdef PRINT_SDCARD_TIMING
  give_audio_buffer_time = (time_us_32() - t0);
#endif

  if (trigger_button_mute) {
    button_mute = true;
    trigger_button_mute = false;
  }

  sync_using_sdcard = false;

  clock_t endTime = time_us_64();
  cpu_utilizations[cpu_utilizations_i] =
      100 * (endTime - startTime) / (US_PER_BLOCK);
  cpu_utilizations_i++;

#ifdef PRINT_AUDIOBLOCKDROPS
  if (sd_card_total_time > 9000) {
    MessageSync_printf(messagesync, "BLOCKDROP: %ld\n", sd_card_total_time);
  }
#endif
  if (cpu_utilizations_i == 64 || sd_card_total_time > 9000 || do_open_file) {
    uint16_t cpu_utilization = 0;
    for (uint8_t i = 0; i < cpu_utilizations_i; i++) {
      cpu_utilization = cpu_utilization + cpu_utilizations[i];
    }
#ifdef PRINT_AUDIO_CPU_USAGE
    uint32_t total_heap = getTotalHeap();
    uint32_t used_heap = total_heap - getFreeHeap();
    MessageSync_printf(messagesync,
                       "cpu [mem]: %2.1f [ %2.1f%% (%ld/%ld)] %d\n",
                       ((float)cpu_utilization) / (float)cpu_utilizations_i,
                       (float)(used_heap) / (float)(total_heap) * 100.0,
                       used_heap, total_heap, buffer->max_sample_count);

#endif
    cpu_utilizations_i = 0;
#ifdef PRINT_SDCARD_TIMING
    MessageSync_printf(messagesync, "sdcard%2.1f %ld %d %d %ld\n",
                       ((float)cpu_utilization) / 64.0, sd_card_total_time,
                       values_to_read, give_audio_buffer_time,
                       take_audio_buffer_time);
#endif
  }
  if (cpu_usage_flag == cpu_usage_flag_limit) {
    cpu_usage_flag = 0;
    reduce_cpu_usage = BLOCKS_PER_SECOND * 30 / sf->bpm_tempo;
    MessageSync_printf(messagesync, "cpu_usage_flag: %d\n", reduce_cpu_usage);
  } else {
    if (cpu_utilizations[cpu_utilizations_i] > cpu_usage_limit_threshold) {
#ifdef PRINT_SDCARD_TIMING
      MessageSync_printf(messagesync, "sdcard%d %ld %d %d %ld\n",
                         cpu_utilizations[cpu_utilizations_i],
                         sd_card_total_time, values_to_read,
                         give_audio_buffer_time, take_audio_buffer_time);
#endif
      cpu_usage_flag++;
      cpu_usage_flag_total++;
#ifdef PRINT_AUDIO_OVERLOADS
      if (cpu_usage_flag_total > 0) {
        clock_t currentTime = time_us_64();
        MessageSync_printf(messagesync, "cpu overloads every: %d ms\n",
                           (currentTime - time_of_initialization) / 1000 /
                               cpu_usage_flag_total);
      }
#endif
      if (cpu_flag_counter == 0) {
        cpu_flag_counter = BLOCKS_PER_SECOND;
      }
      // char fx_string[17];
      // for (uint8_t i = 0; i < 16; i++) {
      //   if (sf->fx_active[i]) {
      //     fx_string[i] = '1';
      //   } else {
      //     fx_string[i] = '0';
      //   }
      // }
      // fx_string[16] = retrig_beat_num > 0 ? '1' : '0';
      // MessageSync_printf(messagesync, "cpu: %d, flag: %d, fx: %s\n",
      //                    cpu_utilizations[cpu_utilizations_i],
      //                    cpu_usage_flag, fx_string);
      // turn off all fx
      for (uint8_t i = 0; i < 16; i++) {
        sf->fx_active[i] = false;
        update_fx(i);
      }
    } else {
      if (cpu_flag_counter > 0) {
        cpu_flag_counter--;
      } else {
        cpu_usage_flag = 0;
      }
    }
  }

  MessageSync_lockIfNotEmpty(messagesync);

  // change phase_forward back if it was switched
  if (change_phase_forward) {
    phase_forward = !phase_forward;
  }
  return;
}
