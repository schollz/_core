// Copyright 2023-2025 Zack Scholl, GPLv3.0

#ifndef LIB_REALTIME_STRETCH
#define LIB_REALTIME_STRETCH 1

#include <stdint.h>

#define REALTIME_STRETCH_Q8_ONE 256u
#define REALTIME_STRETCH_Q8_BYPASS ((11u * REALTIME_STRETCH_Q8_ONE + 5u) / 10u)
#define REALTIME_STRETCH_Q8_MAX (10u * REALTIME_STRETCH_Q8_ONE)
#define REALTIME_STRETCH_GRAIN_LENGTH 2048u
#define REALTIME_STRETCH_GRAIN_HOP 1024u
#define REALTIME_STRETCH_GRAIN_HOP_SHIFT 10u
#define REALTIME_STRETCH_PHASE_INC_Q32 (1ull << 32u)
#define REALTIME_STRETCH_MAX_SOURCE_FRAMES (SAMPLES_PER_BUFFER * 4u + 4u)

typedef struct RealtimeStretchGrain {
  uint64_t start_phase_q32;
  uint64_t phase_inc_q32;
  uint16_t age;
} RealtimeStretchGrain;

uint32_t realtime_stretch_q8 = REALTIME_STRETCH_Q8_ONE;
uint32_t realtime_stretch_applied_q8 = REALTIME_STRETCH_Q8_ONE;
bool realtime_stretch_active = false;
bool realtime_stretch_grains_initialized = false;
uint64_t realtime_stretch_phase_q32 = 0;
RealtimeStretchGrain realtime_stretch_grains[2] = {
    {0, REALTIME_STRETCH_PHASE_INC_Q32, 0},
    {0, REALTIME_STRETCH_PHASE_INC_Q32, REALTIME_STRETCH_GRAIN_HOP}};
static int16_t realtime_stretch_readbuf[2][REALTIME_STRETCH_MAX_SOURCE_FRAMES *
                                           2];

uint32_t realtime_stretch_from_knob_q8(uint16_t knob) {
  if (knob > 4095) {
    knob = 4095;
  }
  const uint64_t eased_knob = (uint64_t)knob * knob * knob;
  const uint64_t eased_max = (uint64_t)4095u * 4095u * 4095u;
  const uint64_t range = REALTIME_STRETCH_Q8_MAX - REALTIME_STRETCH_Q8_ONE;
  return (uint32_t)(REALTIME_STRETCH_Q8_ONE +
                    ((eased_knob * range) + (eased_max / 2u)) / eased_max);
}

void set_realtime_stretch_q8(uint32_t stretch_q8) {
  if (stretch_q8 > REALTIME_STRETCH_Q8_MAX) {
    stretch_q8 = REALTIME_STRETCH_Q8_MAX;
  }
  realtime_stretch_q8 = stretch_q8;
}

void set_realtime_stretch_knob(uint16_t knob) {
  set_realtime_stretch_q8(realtime_stretch_from_knob_q8(knob));
}

bool realtime_stretch_is_active() { return realtime_stretch_active; }

bool realtime_stretch_target_is_active() {
  return realtime_stretch_q8 >= REALTIME_STRETCH_Q8_BYPASS;
}

uint32_t realtime_stretch_bytes_per_frame() {
  SampleInfo *si = banks[sel_bank_cur]->sample[sel_sample_cur].snd[FILEZERO];
  return (uint32_t)(si->num_channels + 1) * sizeof(int16_t);
}

uint32_t realtime_stretch_channels() {
  return banks[sel_bank_cur]->sample[sel_sample_cur].snd[FILEZERO]
             ->num_channels +
         1;
}

uint32_t realtime_stretch_sample_bytes() {
  return banks[sel_bank_cur]->sample[sel_sample_cur].snd[FILEZERO]->size *
         sel_variation_scale[sel_variation];
}

uint32_t realtime_stretch_frame_count() {
  const uint32_t bytes_per_frame = realtime_stretch_bytes_per_frame();
  if (bytes_per_frame == 0) {
    return 0;
  }
  uint32_t frames = realtime_stretch_sample_bytes() / bytes_per_frame;
  if (frames == 0) {
    frames = 1;
  }
  return frames;
}

uint32_t realtime_stretch_preroll_bytes() {
  SampleInfo *si = banks[sel_bank_cur]->sample[sel_sample_cur].snd[FILEZERO];
  return (uint32_t)(si->num_channels + 1) * (si->oversampling + 1) * 44100u;
}

uint64_t realtime_stretch_wrap_phase(uint64_t phase_q32) {
  const uint32_t frame_count = realtime_stretch_frame_count();
  if (frame_count == 0) {
    return 0;
  }
  const uint64_t loop_len_q32 = (uint64_t)frame_count << 32u;
  while (phase_q32 >= loop_len_q32) {
    phase_q32 -= loop_len_q32;
  }
  return phase_q32;
}

uint64_t realtime_stretch_subtract_phase(uint64_t phase_q32,
                                         uint64_t amount_q32) {
  const uint32_t frame_count = realtime_stretch_frame_count();
  if (frame_count == 0) {
    return 0;
  }
  const uint64_t loop_len_q32 = (uint64_t)frame_count << 32u;
  while (amount_q32 >= loop_len_q32) {
    amount_q32 -= loop_len_q32;
  }
  if (phase_q32 >= amount_q32) {
    return phase_q32 - amount_q32;
  }
  return loop_len_q32 - (amount_q32 - phase_q32);
}

void realtime_stretch_invalidate_grains() {
  realtime_stretch_grains_initialized = false;
}

void realtime_stretch_sync_phase_to_playback() {
  const uint32_t bytes_per_frame = realtime_stretch_bytes_per_frame();
  if (bytes_per_frame == 0) {
    phases[0] = 0;
  } else {
    realtime_stretch_phase_q32 =
        realtime_stretch_wrap_phase(realtime_stretch_phase_q32);
    phases[0] = (int32_t)((realtime_stretch_phase_q32 >> 32u) *
                          bytes_per_frame);
  }
  phases[1] = phases[0];
  phase_change = false;
}

void realtime_stretch_reset_from_playback_phase() {
  const uint32_t bytes_per_frame = realtime_stretch_bytes_per_frame();
  uint32_t frame = 0;
  if (bytes_per_frame > 0 && phases[0] > 0) {
    frame = (uint32_t)phases[0] / bytes_per_frame;
  }
  const uint32_t frame_count = realtime_stretch_frame_count();
  if (frame_count > 0) {
    frame %= frame_count;
  }
  realtime_stretch_phase_q32 = (uint64_t)frame << 32u;
  realtime_stretch_invalidate_grains();
}

void realtime_stretch_update_state() {
  if (!realtime_stretch_target_is_active()) {
    if (realtime_stretch_active) {
      realtime_stretch_sync_phase_to_playback();
      realtime_stretch_invalidate_grains();
    }
    realtime_stretch_active = false;
    realtime_stretch_applied_q8 = REALTIME_STRETCH_Q8_ONE;
    return;
  }

  if (!realtime_stretch_active) {
    realtime_stretch_reset_from_playback_phase();
  }
  realtime_stretch_active = true;
  realtime_stretch_applied_q8 = realtime_stretch_q8;
}

void realtime_stretch_initialize_grains(uint64_t grain_phase_inc_q32) {
  realtime_stretch_phase_q32 =
      realtime_stretch_wrap_phase(realtime_stretch_phase_q32);
  const uint64_t previous_grain_offset =
      (uint64_t)REALTIME_STRETCH_GRAIN_HOP * grain_phase_inc_q32;
  realtime_stretch_grains[0] =
      (RealtimeStretchGrain){realtime_stretch_phase_q32, grain_phase_inc_q32,
                             0};
  realtime_stretch_grains[1] = (RealtimeStretchGrain){
      realtime_stretch_subtract_phase(realtime_stretch_phase_q32,
                                      previous_grain_offset),
      grain_phase_inc_q32, REALTIME_STRETCH_GRAIN_HOP};
  realtime_stretch_grains_initialized = true;
}

uint32_t realtime_stretch_grain_window(uint16_t age) {
  if (age >= REALTIME_STRETCH_GRAIN_LENGTH) {
    return 0;
  }
  if (age <= REALTIME_STRETCH_GRAIN_HOP) {
    return age;
  }
  return REALTIME_STRETCH_GRAIN_LENGTH - age;
}

bool realtime_stretch_read_frames(uint64_t start_phase_q32,
                                  uint32_t frames_to_read, int16_t *dst) {
  const uint32_t channels = realtime_stretch_channels();
  const uint32_t frame_count = realtime_stretch_frame_count();
  const uint32_t bytes_per_frame = realtime_stretch_bytes_per_frame();
  if (frame_count == 0 || bytes_per_frame == 0 || frames_to_read == 0) {
    return false;
  }

  uint32_t frame = (uint32_t)(realtime_stretch_wrap_phase(start_phase_q32) >>
                              32u);
  uint32_t frames_done = 0;
  while (frames_done < frames_to_read) {
    uint32_t frames_now = frame_count - frame;
    if (frames_now > frames_to_read - frames_done) {
      frames_now = frames_to_read - frames_done;
    }

    const uint32_t phase_bytes = frame * bytes_per_frame;
    const uint32_t offset =
        WAV_HEADER + realtime_stretch_preroll_bytes() + phase_bytes;
    if (f_lseek(&fil_current, offset) != FR_OK) {
      return false;
    }

    unsigned int bytes_read = 0;
    const uint32_t bytes_to_read = frames_now * bytes_per_frame;
    if (f_read(&fil_current, &dst[frames_done * channels], bytes_to_read,
               &bytes_read) != FR_OK) {
      return false;
    }
    if (bytes_read < bytes_to_read) {
      uint32_t samples_read = bytes_read / sizeof(int16_t);
      uint32_t samples_requested = bytes_to_read / sizeof(int16_t);
      for (uint32_t i = samples_read; i < samples_requested; i++) {
        dst[frames_done * channels + i] = 0;
      }
    }

    frames_done += frames_now;
    frame = 0;
  }
  return true;
}

int16_t realtime_stretch_interpolated_frame(const int16_t *src,
                                            uint32_t frame_offset,
                                            uint32_t channel,
                                            uint32_t channels,
                                            uint32_t frac) {
  const int32_t a = src[frame_offset * channels + channel];
  const int32_t b = src[(frame_offset + 1) * channels + channel];
  const int32_t diff = b - a;
  return (int16_t)(a + (int32_t)(((int64_t)diff * frac) >> 32u));
}

void realtime_stretch_advance_grains(uint32_t rendered,
                                     uint64_t grain_phase_inc_q32) {
  realtime_stretch_phase_q32 = realtime_stretch_wrap_phase(
      realtime_stretch_phase_q32 +
      (((uint64_t)rendered * grain_phase_inc_q32) << 8u) /
          realtime_stretch_applied_q8);

  for (uint8_t grain_index = 0; grain_index < 2; grain_index++) {
    RealtimeStretchGrain *grain = &realtime_stretch_grains[grain_index];
    grain->age += rendered;
    if (grain->age >= REALTIME_STRETCH_GRAIN_LENGTH) {
      grain->start_phase_q32 = realtime_stretch_phase_q32;
      grain->phase_inc_q32 = grain_phase_inc_q32;
      grain->age = 0;
    }
  }
}

bool realtime_stretch_render(int16_t *values, uint32_t sample_count,
                             uint64_t grain_phase_inc_q32) {
  if (grain_phase_inc_q32 == 0) {
    grain_phase_inc_q32 = REALTIME_STRETCH_PHASE_INC_Q32;
  }
  const uint64_t max_phase_inc_q32 = 4ull << 32u;
  if (grain_phase_inc_q32 > max_phase_inc_q32) {
    grain_phase_inc_q32 = max_phase_inc_q32;
  }

  if (!realtime_stretch_grains_initialized) {
    realtime_stretch_initialize_grains(grain_phase_inc_q32);
  }

  const uint32_t channels = realtime_stretch_channels();
  for (uint32_t i = 0; i < sample_count * 2; i++) {
    values[i] = 0;
  }

  uint32_t rendered = 0;
  while (rendered < sample_count) {
    uint32_t segment = sample_count - rendered;
    for (uint8_t grain_index = 0; grain_index < 2; grain_index++) {
      uint32_t remaining =
          REALTIME_STRETCH_GRAIN_LENGTH -
          realtime_stretch_grains[grain_index].age;
      if (remaining < segment) {
        segment = remaining;
      }
    }

    for (uint8_t grain_index = 0; grain_index < 2; grain_index++) {
      RealtimeStretchGrain *grain = &realtime_stretch_grains[grain_index];
      const uint64_t start_phase_q32 =
          grain->start_phase_q32 +
          (uint64_t)grain->age * grain->phase_inc_q32;
      const uint64_t local_span_q32 =
          (start_phase_q32 & 0xffffffffull) +
          (uint64_t)(segment - 1) * grain->phase_inc_q32;
      uint32_t frames_to_read =
          (uint32_t)((local_span_q32 >> 32u) + 3u);
      if (frames_to_read > REALTIME_STRETCH_MAX_SOURCE_FRAMES) {
        frames_to_read = REALTIME_STRETCH_MAX_SOURCE_FRAMES;
      }
      if (!realtime_stretch_read_frames(start_phase_q32, frames_to_read,
                                        realtime_stretch_readbuf[grain_index])) {
        return false;
      }
    }

    for (uint32_t i = 0; i < segment; i++) {
      int32_t mixed[2] = {0, 0};
      for (uint8_t grain_index = 0; grain_index < 2; grain_index++) {
        RealtimeStretchGrain *grain = &realtime_stretch_grains[grain_index];
        const uint32_t weight =
            realtime_stretch_grain_window(grain->age + i);
        if (weight == 0) {
          continue;
        }
        const uint64_t segment_start_phase_q32 =
            grain->start_phase_q32 +
            (uint64_t)grain->age * grain->phase_inc_q32;
        const uint64_t local_phase_q32 =
            (segment_start_phase_q32 & 0xffffffffull) +
            (uint64_t)i * grain->phase_inc_q32;
        const uint32_t frame_offset = (uint32_t)(local_phase_q32 >> 32u);
        const uint32_t frac = (uint32_t)local_phase_q32;
        for (uint8_t channel = 0; channel < 2; channel++) {
          const uint8_t source_channel = channels == 1 ? 0 : channel;
          mixed[channel] +=
              (int32_t)realtime_stretch_interpolated_frame(
                  realtime_stretch_readbuf[grain_index], frame_offset,
                  source_channel, channels, frac) *
              (int32_t)weight;
        }
      }

      for (uint8_t channel = 0; channel < 2; channel++) {
        values[(rendered + i) * 2 + channel] =
            (int16_t)(mixed[channel] >> REALTIME_STRETCH_GRAIN_HOP_SHIFT);
      }
    }

    realtime_stretch_advance_grains(segment, grain_phase_inc_q32);
    rendered += segment;
  }

  realtime_stretch_sync_phase_to_playback();
  realtime_stretch_active = true;
  return true;
}

#endif
