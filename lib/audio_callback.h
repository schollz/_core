// Copyright 2023-2025 Zack Scholl, GPLv3.0
#include "audio_profile.h"
// previous output per channel (persistent state)
#ifdef INCLUDE_EZEPTOCORE
#define BASS_LP_A 64917  // alpha ≈ 0.9908
#define BASS_LP_B 619    // 1 - alpha
static int32_t smear_buf[32][2];
static uint8_t smear_pos = 0;
// auto-oscillation state
static uint8_t smear_amt = 1;
static int8_t smear_dir = 1;
static uint32_t smear_acc = 0;
static int32_t bass_lp[2] = {0, 0};
#define MICRO_DELAY_MAX 64  // ~1.45 ms @ 44.1kHz

static int32_t tremble_val[2] = {Q16_16_1, Q16_16_1};
static int32_t tremble_vel[2] = {0, 0};

#endif
uint8_t cpu_utilizations[64];
uint8_t cpu_utilizations_i = 0;
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
#include "audio_resample.h"
int16_t newArray[SAMPLES_PER_BUFFER];
// Core 1's stack is reserved for control flow and short-lived source buffers.
// This always-present 3.5 KiB render workspace is static so normal stereo
// playback fits within the RP2040 scratch-bank stack.
static int32_t audio_render_samples[SAMPLES_PER_BUFFER * 2];

void __not_in_flash_func(array_resample_linear441)(const int16_t *arr,
                                                   uint32_t arr_size,
                                                   uint32_t stride) {
  AP_MEASURE(AP_RESAMPLE,audio_resample_linear(newArray, arr, arr_size, SAMPLES_PER_BUFFER, stride));
}

void __not_in_flash_func(process_source_fx)(int16_t *values,
                                            uint32_t values_len) {
  // beat repeat
  AP_MEASURE(AP_BEAT,BeatRepeat_process(beatrepeat, values, values_len));
  AP_START(ap_source);

  // saturate before resampling?
  if (sf->fx_active[FX_SATURATE]) {
    for (uint16_t i = 0; i < values_len; i++) {
      values[i] = values[i] * sf->fx_param[FX_SATURATE][0] / 128;
    }
    if (audio_variant_num > 0) {
      set_audio_variant(sf->fx_param[FX_SATURATE][1] * audio_variant_num / 256);
    }
    Saturation_process(saturation, values, values_len);
  }

  // shaper
  AP_END(AP_SATURATE,ap_source);AP_RESET(ap_source);
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

  AP_END(AP_SHAPER,ap_source);AP_RESET(ap_source);
  if (sf->fx_active[FX_FUZZ]) {
    Fuzz_process(values, values_len, sf->fx_param[FX_FUZZ][0],
                 sf->fx_param[FX_FUZZ][1]);
  }

  // bitcrush
  AP_END(AP_FUZZ,ap_source);AP_RESET(ap_source);
  if (sf->fx_active[FX_BITCRUSH]) {
    Bitcrush_process(values, values_len, sf->fx_param[FX_BITCRUSH][0],
                     sf->fx_param[FX_BITCRUSH][1]);
  }
  AP_END(AP_BITCRUSH,ap_source);
}

#ifdef DEBUG_AUDIO_WITH_SINE_WAVE
uint32_t sine_wave_counter = 0;
#endif

static void __not_in_flash_func(zeptocore_render_audio)() {
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
    ZD_CALL(zd_audio_counter(ZD_NO_BUFFER));
    return;
  }

  int32_t *samples = audio_render_samples;
  ZD_WITNESS_RENDERED();
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
  ZD_CALL(zd_audio_output(buffer->sample_count, 0));
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
    ZD_CALL(zd_audio_output(buffer->sample_count, 0));
  give_audio_buffer(ap, buffer);

    ZD_CALL(zd_audio_counter(ZD_MUTED));
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

  bool realtime_stretch_was_active = realtime_stretch_is_active();
  realtime_stretch_update_state();
  if (realtime_stretch_was_active && !realtime_stretch_is_active()) {
    last_seeked = 1;
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
  bool allow_file_change=true;
#if AUDIO_PREPARE_NEXT
  if(fil_current_change||fil_current_change_force) {
    char next_path[32];
    format_sample_filename(next_path,sel_bank_next,
        sel_sample_next%banks[sel_bank_next]->num_samples,
        sel_variation+audio_variant*2);
    allow_file_change=audio_prepare_ready(next_path);
  }
#endif
  if (allow_file_change && (fil_current_change || fil_current_change_force)) {
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
  // Normal and stretch rendering are mutually exclusive. Reuse the existing
  // fixed stretch read workspace instead of pitch-sized stack arrays. Bound
  // extreme combined rates to its capacity, including interpolation lookahead.
  if (!realtime_stretch_is_active()) {
    samples_to_read = audio_source_frame_limit(samples_to_read,
        sizeof realtime_stretch_readbuf / sizeof realtime_stretch_readbuf[0],
        banks[sel_bank_cur]->sample[sel_sample_cur].snd[FILEZERO]->num_channels + 1);
  }
  uint64_t realtime_stretch_grain_phase_inc_q32 =
      (((uint64_t)samples_to_read) << 32u) / buffer->max_sample_count;

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
  int32_t vol_main =
      round((float)volume_vals[sf->vol] * retrig_vol * envelope_volume_val);

  if (!phase_change && !realtime_stretch_is_active()) {
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

  if (realtime_stretch_is_active()) {
    if (phase_change) {
      phases[1] = phases[0];
      phases[0] = phase_new;
      phase_change = false;
      realtime_stretch_reset_from_playback_phase();
    }

    if (do_open_file) {
      sel_bank_cur = sel_bank_next;
      sel_sample_cur = sel_sample_next % banks[sel_bank_cur]->num_samples;

      t0 = time_us_32();
      format_sample_filename(fil_current_name, sel_bank_cur, sel_sample_cur,
                             sel_variation + audio_variant * 2);
      audio_file_open(fil_current_name);
      t1 = time_us_32();
      sd_card_total_time += (t1 - t0);
      do_open_file = false;
      realtime_stretch_reset_from_playback_phase();
    }

    int16_t stretch_values[buffer->max_sample_count * 2];
    AP_START(ap_stretch);
    bool stretch_ok=realtime_stretch_render(stretch_values, buffer->max_sample_count,
                                 realtime_stretch_grain_phase_inc_q32);
    AP_END(AP_STRETCH,ap_stretch);
    if (!stretch_ok) {
      for (uint16_t i = 0; i < buffer->max_sample_count; i++) {
        samples[i * 2 + 0] = 0;
        samples[i * 2 + 1] = 0;
      }
      realtime_stretch_invalidate_grains();
    } else {
      ZD_CALL(buffer->user_data=zd_switch_render_tag());
      process_source_fx(stretch_values, buffer->max_sample_count * 2);
      for (uint16_t i = 0; i < buffer->max_sample_count; i++) {
        for (uint8_t channel = 0; channel < 2; channel++) {
          int16_t value = stretch_values[i * 2 + channel];
          if (do_fade_out) {
            value = crossfade3_out(value, i, CROSSFADE3_COS);
          } else if (do_fade_in) {
            value = crossfade3_in(value, i, CROSSFADE3_COS);
          }
          samples[i * 2 + channel] =
              q16_16_multiply(((int32_t)value) << 16, vol_main);
        }
      }
    }
    last_seeked = 1;
    goto AUDIO_SOURCE_RENDERED;
  }

  {
    int16_t *values = realtime_stretch_readbuf;
  for (int8_t head = 1; head >= 0; head--) {
    if (head == 1 && (!do_crossfade || do_fade_in)) {
      continue;
    }

    if (head == 0 && do_open_file) {
      // setup the next
      sel_bank_cur = sel_bank_next;
      sel_sample_cur = sel_sample_next % banks[sel_bank_cur]->num_samples;
      t0 = time_us_32();
        format_sample_filename(fil_current_name, sel_bank_cur, sel_sample_cur,
                               sel_variation + audio_variant * 2);
        audio_file_open(fil_current_name);
      t1 = time_us_32();
      sd_card_total_time += (t1 - t0);
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
        }
      }
#endif
      FRESULT seek_result=zd_f_lseek(&fil_current,
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
                          PHASE_DIVISOR, ZD_SEEK);
      if(seek_result!=FR_OK) {
        audio_media_io_failed(seek_result);
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
        ZD_CALL(zd_audio_output(buffer->sample_count, 0));
  give_audio_buffer(ap, buffer);
        sync_using_sdcard = false;
        ZD_CALL(zd_audio_counter(ZD_ERROR_SILENCE));
        // sdcard_startup();
        return;
      }
      t1 = time_us_32();
      sd_card_total_time += (t1 - t0);
    } else {
      ZD_CALL(zd_audio_counter(ZD_SEEK_SKIPPED));
    }

    t0 = time_us_32();
    FRESULT read_result=zd_f_read(&fil_current,values,values_to_read,&fil_bytes_read,ZD_READ);
    if(read_result!=FR_OK) {
      audio_media_io_failed(read_result);
      memset(values,0,values_to_read);fil_bytes_read=0;
    }
    if(head==0 && read_result==FR_OK && fil_bytes_read)
      ZD_CALL(buffer->user_data=zd_switch_render_tag());
    t1 = time_us_32();
    sd_card_total_time += (t1 - t0);
    last_seeked = phases[head] + fil_bytes_read;

    if (!phase_forward) {
      // reverse audio
      for (int i = 0; i < values_len / 2; i++) {
        int16_t temp = values[i];
        values[i] = values[values_len - i - 1];
        values[values_len - i - 1] = temp;
      }
    }

    process_source_fx(values, values_len);

    if (banks[sel_bank_cur]
            ->sample[sel_sample_cur]
            .snd[FILEZERO]
            ->num_channels == 0) {
      // mono
      array_resample_linear441(values, samples_to_read, 1);

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
        array_resample_linear441(values + channel, samples_to_read, 2);

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
  }

AUDIO_SOURCE_RENDERED:

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
  AP_START(ap_filter);
#ifdef INCLUDE_FILTER
  for (uint8_t channel = 0; channel < 2; channel++) {
    ResonantFilter_update(resFilter[channel], samples, buffer->max_sample_count,
                          channel);
  }
#endif

  // apply other fx
  AP_END(AP_FILTER,ap_filter);
  AP_START(ap_pan);
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
  AP_END(AP_PAN,ap_pan);
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
      AP_MEASURE(AP_REVERB,FV_Reverb_process(freeverb, samples, buffer->max_sample_count));

      if (first_loop_ever) {
        first_loop_ever = false;
      }
    }
  } else {
#ifdef INCLUDE_ECTOCORE
    if (sf->fx_active[FX_DELAY]) {
      AP_START(ap_delay_setup);
      Delay_setFeedbackf(delay,
                         Range(LFNoise2_period(noise_feedback, 1), 0.49, 0.99));
      float v = Range(LFNoise2_period(noise_duration, 2), 100, 10000);
      Delay_setDuration(delay, v);
      AP_END(AP_DELAY_SETUP,ap_delay_setup);
      // float v = Range(LFNoise2_period(noise_duration, 2), 6.64f, 13.28f);
      // Delay_setDuration(delay, powf(2, v));
    }
    AP_MEASURE(AP_DELAY,Delay_process(delay, samples, buffer->max_sample_count, 0));
#else
    if (sf->fx_active[FX_DELAY] && sf->fx_param[FX_DELAY][2] > 30) {
      AP_START(ap_delay_setup);
      Delay_setFeedbackf(delay,
                         Range(LFNoise2_period(noise_feedback, 2), 0.49, 0.99));
      float v = Range(LFNoise2_period(noise_duration, 2), 100, 10000);
      Delay_setDuration(delay, v);
      // float v = Range(LFNoise2_period(noise_duration, 2), 6.64f, 13.28f);
      // // raise v from the 10th power
      // Delay_setDuration(delay, powf(2, v));
      AP_END(AP_DELAY_SETUP,ap_delay_setup);
    }
    AP_MEASURE(AP_DELAY,Delay_process(delay, samples, buffer->max_sample_count, 0));
#endif
  }

  // apply comb
  AP_MEASURE(AP_COMB,Comb_process(combfilter, samples, buffer->max_sample_count));

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

  AP_START(ap_digital);
#ifdef INCLUDE_EZEPTOCORE
  if (mode_amiga_index > 5) {
    int32_t held[2] = {0, 0};
    uint8_t hold_len = mode_amiga_index - 5;
    for (uint16_t i = 0; i < buffer->max_sample_count; i++) {
      if ((i % hold_len) == 0) {
        // compute new filtered value
        for (uint8_t j = 0; j < 2; j++) {
          // convert 32-bit to 8-bit (Amiga-style)
          held[j] = (int32_t)((int8_t)(samples[i * 2 + j] >> 24)) << 24;
        }
      }

      // sample & hold
      samples[i * 2 + 0] = held[0];
      samples[i * 2 + 1] = held[1];
    }
  }

  if (mode_digital_saturation > 0) {
    uint8_t amt = mode_digital_saturation;
    if (amt > 100)
      amt = 100;

    /*
      Threshold mapping (Q16.16)

      amt = 0   → threshold ≈ full scale
      amt = 100 → threshold ≈ small → heavy folding

      Use a nonlinear curve so low values are gentle
    */
    int32_t thresh = 0x7FFF0000 >> (1 + (amt * 5) / 100);  // shift 1..6

    for (uint16_t i = 0; i < buffer->max_sample_count; i++) {
      for (uint8_t j = 0; j < 2; j++) {
        int32_t x = samples[i * 2 + j];

        // wavefold
        if (x > thresh) {
          x = thresh - (x - thresh);
        } else if (x < -thresh) {
          x = -thresh - (x + thresh);
        }

        samples[i * 2 + j] = x;
      }
    }
  }

  if (mode_chaos_trembler > 0) {
    uint8_t amt = mode_chaos_trembler;
    if (amt > 100)
      amt = 100;

    /*
      Mapping:
      amt → chaos strength & speed
    */
    int32_t chaos_gain = (amt << 16) / 120;  // depth
    int32_t chaos_speed = 1 + (amt >> 4);    // update aggressiveness

    for (uint16_t i = 0; i < buffer->max_sample_count; i++) {
      for (uint8_t ch = 0; ch < 2; ch++) {
        /* -------- chaotic integrator -------- */

        // random force
        int32_t noise = ((rand() & 0xFF) - 128) << 9;

        tremble_vel[ch] += q16_16_multiply(noise, chaos_gain);
        tremble_vel[ch] -= tremble_vel[ch] >> chaos_speed;

        tremble_val[ch] += tremble_vel[ch];

        /* -------- clamp tremble value -------- */

        if (tremble_val[ch] > (Q16_16_1 + chaos_gain))
          tremble_val[ch] = Q16_16_1 + chaos_gain;
        if (tremble_val[ch] < (Q16_16_1 - chaos_gain))
          tremble_val[ch] = Q16_16_1 - chaos_gain;

        /* -------- apply modulation -------- */

        samples[i * 2 + ch] =
            q16_16_multiply(samples[i * 2 + ch], tremble_val[ch]);
      }
    }
  }

  if (mode_digital_smear > 0) {
    uint8_t speed = mode_digital_smear;
    if (speed > 100)
      speed = 100;

    /*
      speed = 1   → VERY slow (multi-second sweep)
      speed = 100 → fast shimmer
    */

    // how often smear_amt advances (samples)
    uint32_t smear_period = 5000 - (speed * 49);  // ~5000 → ~100

    if (smear_period < 256)
      smear_period = 256;

    for (uint16_t i = 0; i < buffer->max_sample_count; i++) {
      smear_acc++;
      if (smear_acc >= smear_period) {
        smear_acc = 0;

        smear_amt += smear_dir;
        if (smear_amt >= 100) {
          smear_amt = 100;
          smear_dir = -1;
        } else if (smear_amt <= 1) {
          smear_amt = 1;
          smear_dir = 1;
        }
      }

      smear_pos = (smear_pos + 1) & 31;

      // derive delay + mix from oscillating amount
      uint8_t delay_len = 1 + (smear_amt * 31) / 100;
      int32_t wet = (smear_amt << 16) / 120;
      int32_t dry = (1 << 16) - wet;

      for (uint8_t j = 0; j < 2; j++) {
        int32_t in = samples[i * 2 + j];

        uint8_t tap = (smear_pos - delay_len) & 31;
        int32_t d = smear_buf[tap][j];

        smear_buf[smear_pos][j] = in;

        samples[i * 2 + j] = q16_16_multiply(in, dry) + q16_16_multiply(d, wet);
      }
    }
  }

  if (mode_digital_jitter > 0) {
    // mode_digital_jitter: 0..100
    uint8_t amt = mode_digital_jitter;
    if (amt > 100)
      amt = 100;

    /*
      Higher amt = more unstable clock
      Probability is VERY small per sample
    */
    uint8_t jitter_prob = amt >> 2;  // 0..25

    for (uint16_t i = 1; i < buffer->max_sample_count - 1; i++) {
      for (uint8_t j = 0; j < 2; j++) {
        int32_t x = samples[i * 2 + j];

        // random clock slip
        uint8_t r = rand() & 31;  // 0..31

        if (r < jitter_prob) {
          // slip backward
          x = samples[(i - 1) * 2 + j];
        } else if (r > (31 - jitter_prob)) {
          // slip forward
          x = samples[(i + 1) * 2 + j];
        }

        samples[i * 2 + j] = x;
      }
    }
  }

  if (mode_digital_bass > 0) {
    // mode_digital_bass: 0..100
    uint8_t amt = mode_digital_bass;
    if (amt > 100)
      amt = 100;

    /* ---------------- mix ---------------- */
    int32_t wet = (amt << 16) / 120;  // stays conservative
    int32_t dry = (1 << 16) - wet;

    /* ---------------- gain ---------------- */
    // 1.0 → ~1.8 max (safe, no clipping)
    int32_t bass_gain = Q16_16_1 + ((amt * Q16_16_1) >> 9);

    /* ---------------- LPF @ ~32 Hz ---------------- */
    // essentially your original cutoff
    const int32_t LP_A = BASS_LP_A;  // 64917
    const int32_t LP_B = BASS_LP_B;  // 619

    for (uint16_t i = 0; i < buffer->max_sample_count; i++) {
      for (uint8_t ch = 0; ch < 2; ch++) {
        int32_t in = samples[i * 2 + ch];

        // sub extraction
        bass_lp[ch] =
            q16_16_multiply(LP_A, bass_lp[ch]) + q16_16_multiply(LP_B, in);

        // headroom trim (~0.85)
        int32_t sub = bass_lp[ch] - (bass_lp[ch] >> 3);

        // controlled boost
        int32_t boosted = q16_16_multiply(sub, bass_gain);

        // clean wet/dry mix
        samples[i * 2 + ch] =
            q16_16_multiply(in, dry) + q16_16_multiply(boosted, wet);
      }
    }
  }

#endif

  AP_END(AP_DIGITAL,ap_digital);
  buffer->sample_count = buffer->max_sample_count;
  AP_START(ap_convert);
  t0 = time_us_32();
  for (uint16_t i = 0; i < buffer->max_sample_count; i++) {
    samples16[i * 2 + 0] = (int16_t)(samples[i * 2 + 0] >> 16);
    samples16[i * 2 + 1] = (int16_t)(samples[i * 2 + 1] >> 16);
  }
  ZD_CALL(zd_audio_output(buffer->sample_count, 0));
  give_audio_buffer(ap, buffer);
#ifdef PRINT_SDCARD_TIMING
  give_audio_buffer_time = (time_us_32() - t0);
#endif

  AP_END(AP_CONVERT,ap_convert);
  if (trigger_button_mute) {
    button_mute = true;
    trigger_button_mute = false;
  }

  sync_using_sdcard = false;

  clock_t endTime = time_us_64();
  cpu_utilizations[cpu_utilizations_i] =
      100 * (endTime - startTime) / (US_PER_BLOCK);
  cpu_utilizations_i++;

  if (cpu_utilizations_i == 64 || sd_card_total_time > 9000 || do_open_file) {
    cpu_utilizations_i = 0;
  }
  if (cpu_usage_flag == cpu_usage_flag_limit) {
    cpu_usage_flag = 0;
    reduce_cpu_usage = BLOCKS_PER_SECOND * 30 / sf->bpm_tempo;
  } else {
    if (cpu_utilizations[cpu_utilizations_i] > cpu_usage_limit_threshold) {
      cpu_usage_flag++;
      cpu_usage_flag_total++;
      if (cpu_flag_counter == 0) {
        cpu_flag_counter = BLOCKS_PER_SECOND;
      }
      // turn off all fx
      for (uint8_t i = 0; i < 16; i++) {
        sf->fx_active[i] = false;
        update_fx(i);
      }
      set_realtime_stretch_q8(REALTIME_STRETCH_Q8_ONE);
    } else {
      if (cpu_flag_counter > 0) {
        cpu_flag_counter--;
      } else {
        cpu_usage_flag = 0;
      }
    }
  }

  // change phase_forward back if it was switched
  if (change_phase_forward) {
    phase_forward = !phase_forward;
  }
#if AUDIO_PREPARE_NEXT
  // The current block is already queued. Keep all SD work on its owning core,
  // and attempt at most one preparation step when this render left headroom.
  if((uint32_t)(time_us_32()-startTime)<US_PER_BLOCK/2&&
     (fil_current_change||fil_current_change_force||do_open_file_ready)) {
    char next_path[32];
    unsigned next_sample=sel_sample_next%banks[sel_bank_next]->num_samples;
    format_sample_filename(next_path,sel_bank_next,next_sample,
                           sel_variation+audio_variant*2);
    if(!audio_prepare_ready(next_path))audio_prepare_step(next_path);
    else if(do_open_file_ready) {
      float ratio=(float)banks[sel_bank_next]->sample[next_sample].snd[FILEZERO]->size/
                   banks[sel_bank_cur]->sample[sel_sample_cur].snd[FILEZERO]->size;
      FSIZE_t next_phase=round((float)phases[0]*ratio*
          sel_variation_scale[sel_variation]*sel_variation_scale[sel_variation]);
      FSIZE_t offset=WAV_HEADER+
          (banks[sel_bank_next]->sample[next_sample].snd[FILEZERO]->num_channels+1)*
          (banks[sel_bank_next]->sample[next_sample].snd[FILEZERO]->oversampling+1)*44100+
          next_phase/PHASE_DIVISOR*PHASE_DIVISOR;
      audio_prepare_warm(offset);
    }
  }
#endif
  return;
}

// All returns from the renderer pass through measurement/publication.
#if AUDIO_DETAILED_TIMING
static uint32_t audio_profile_effects(void) {
  uint32_t mask=0;
  if(sf)for(unsigned i=0;i<16;++i)mask|=sf->fx_active[i]?1u<<i:0;
  return mask;
}
static uint32_t audio_profile_source(bool owns) {
  return (sel_bank_cur<<12)|(sel_sample_cur<<8)|(sel_variation+audio_variant*2)|
      ((owns&&fil_current.cltbl)?1u<<30:0)|(audio_callback_in_mute?1u<<31:0);
}
#endif
#if AUDIO_EXTRA_OUTPUT_BUFFER
static volatile uint32_t audio_queue_refills;
static unsigned audio_queued_blocks(void) {
  if(!ap)return 0;
  uint32_t irq=spin_lock_blocking(ap->prepared_list_spin_lock);
  unsigned count=0;
  for(audio_buffer_t *b=ap->prepared_list;b&&count<4;b=b->next)++count;
  spin_unlock(ap->prepared_list_spin_lock,irq);
  return count;
}
#endif
void __not_in_flash_func(i2s_callback_func)() {
  ZD_CALL(zd_audio_begin());
  bool owns_media=audio_media_begin();
  AP_CALL(audio_profile_begin(owns_media&&zeptocore_diag.header[21]>=3,
      audio_profile_source(owns_media),audio_profile_effects(),
      sf?sf->pitch_val_index:0,realtime_stretch_q8));
  if(owns_media) {
    zeptocore_render_audio();
#if AUDIO_EXTRA_OUTPUT_BUFFER
    // Maintain one queued block beyond the ordinary next block. A startup
    // reserve alone can drain and never recover with one render per DMA event.
    // No filesystem work occurs under the queue's short list lock.
    if(ap&&audio_queued_blocks()<2) {
      ++audio_queue_refills;
      zeptocore_render_audio();
    }
#endif
  } else if(ap) {
    audio_buffer_t *buffer=take_audio_buffer(ap,false);
    if(buffer) {
      ZD_WITNESS_RENDERED();
      memset(buffer->buffer->bytes,0,buffer->max_sample_count*4);
      buffer->sample_count=buffer->max_sample_count;
      ZD_CALL(zd_audio_output(buffer->sample_count,0));
      ZD_CALL(zd_audio_counter(ZD_MUTED));
      give_audio_buffer(ap,buffer);
    } else ZD_CALL(zd_audio_counter(ZD_NO_BUFFER));
  }
  AP_CALL(audio_profile_end(audio_profile_source(owns_media),audio_profile_effects()));
  ZD_CALL(if(zeptocore_diag.request.sequence!=zeptocore_diag.audio.header.sequence) {
    zd_audio.counters[12]=audio_file_generation;
    zd_audio.counters[13]=seek_maps_stats.hits;
    zd_audio.counters[14]=seek_maps_stats.misses;
    zd_audio.counters[15]=audio_media_stats.withheld_callbacks;
    zd_audio.context[15]=owns_media?fil_is_open:2;
  });
  ZD_CALL(if (owns_media && zeptocore_diag.request.sequence !=
                 zeptocore_diag.audio.header.sequence && fil_is_open) {
    uint32_t *c = zd_audio.context;
    c[0] = sel_bank_cur; c[1] = sel_sample_cur;
    c[2] = sel_variation; c[3] = audio_variant; c[4] = phase_forward;
    c[5] = realtime_stretch_q8; c[6] = sf->pitch_val_index;
    c[7] = sf->bpm_tempo; c[8] = 0;
    for (unsigned i = 0; i < 16; ++i) c[8] |= sf->fx_active[i] ? 1u << i : 0;
    c[9] = f_size(&fil_current); c[10] = f_size(&fil_current) >> 32;
    c[11] = fil_current.obj.sclust;
    c[12] = f_tell(&fil_current); c[13] = f_tell(&fil_current) >> 32;
    c[14] = fil_current.cltbl != NULL; c[15] = fil_is_open;
  });
  ZD_CALL(zd_audio_end());
  bool quiescent=false;
  // Optional map work may use a stopped, already muted output with no effect
  // tails. A mute flag alone cannot establish this, or filesystem ownership.
  quiescent=owns_media&&playback_stopped&&audio_callback_in_mute&&
      !sf->fx_active[FX_EXPAND]&&delay&&!delay->on;
#ifdef INCLUDE_SINEBASS
  if(quiescent&&wavebass) {
    quiescent=wavebass->change_count>=2000;
    for(unsigned voice=0;voice<WAVETABLEBASS_MAX&&quiescent;++voice)
      for(unsigned osc=0;osc<WAVETABLESYN_MAX;++osc)
        if(wavebass->osc[voice]->active[osc])quiescent=false;
  }
#endif
  audio_media_end(quiescent);
}
