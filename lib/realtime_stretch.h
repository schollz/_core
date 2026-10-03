// Copyright 2023-2026 Zack Scholl, GPLv3.0
#ifndef LIB_REALTIME_STRETCH
#define LIB_REALTIME_STRETCH 1
#include <stdint.h>

#define REALTIME_STRETCH_Q8_ONE 256u
#define REALTIME_STRETCH_Q8_BYPASS ((11u * REALTIME_STRETCH_Q8_ONE + 5u) / 10u)
#define REALTIME_STRETCH_Q8_MAX (16u * REALTIME_STRETCH_Q8_ONE)
#define REALTIME_STRETCH_Q8_EFFECT (8u * REALTIME_STRETCH_Q8_ONE)
#define REALTIME_STRETCH_GRAIN_LENGTH 2048u
#define REALTIME_STRETCH_GRAIN_HOP 1024u
#define REALTIME_STRETCH_GRAIN_HOP_SHIFT 10u
#define REALTIME_STRETCH_PHASE_INC_Q32 (1ull << 32u)
#define REALTIME_STRETCH_MAX_SOURCE_FRAMES (SAMPLES_PER_BUFFER * 4u + 4u)
#define REALTIME_STRETCH_TRANSITION_FRAMES 64u

typedef struct RealtimeStretchGrain {
  int64_t start_phase_q32;
  uint64_t phase_inc_q32;
  uint16_t age;
} RealtimeStretchGrain;
typedef struct RealtimeStretchBounds {
  uint32_t first, last, loop_first, loop_last;
  bool stop;
} RealtimeStretchBounds;

// The manual control survives activation/deactivation of the sequenced effect.
uint32_t realtime_stretch_q8 = REALTIME_STRETCH_Q8_ONE;
uint32_t realtime_stretch_applied_q8 = REALTIME_STRETCH_Q8_ONE;
bool realtime_stretch_active = false;
bool realtime_stretch_grains_initialized = false;
bool realtime_stretch_forward = true;
uint64_t realtime_stretch_phase_q32 = 0;
RealtimeStretchGrain realtime_stretch_grains[2];
// Shared with normal playback. Stretch amount never changes this allocation.
static int16_t realtime_stretch_readbuf[REALTIME_STRETCH_MAX_SOURCE_FRAMES * 2];
static int32_t realtime_stretch_last[2], realtime_stretch_transition_from[2];
static uint32_t realtime_stretch_transition_remaining;

static void realtime_stretch_begin_transition(void) {
  for (unsigned c = 0; c < 2; ++c)
    realtime_stretch_transition_from[c] = realtime_stretch_last[c];
  realtime_stretch_transition_remaining = REALTIME_STRETCH_TRANSITION_FRAMES;
}
// Applied to both normal and granular source output, before the common FX.
static void realtime_stretch_declick(int32_t* samples, uint32_t count) {
  uint32_t n = count < realtime_stretch_transition_remaining
                   ? count
                   : realtime_stretch_transition_remaining;
  for (uint32_t i = 0; i < n; ++i) {
    unsigned old = --realtime_stretch_transition_remaining;
    for (unsigned c = 0; c < 2; ++c)
      samples[2 * i + c] =
          (int32_t)(((int64_t)realtime_stretch_transition_from[c] * old +
                     (int64_t)samples[2 * i + c] *
                         (REALTIME_STRETCH_TRANSITION_FRAMES - old)) /
                    REALTIME_STRETCH_TRANSITION_FRAMES);
  }
  if (count)
    for (unsigned c = 0; c < 2; ++c)
      realtime_stretch_last[c] = samples[2 * (count - 1) + c];
}
uint32_t realtime_stretch_from_knob_q8(uint16_t knob) {
  if (knob > 4095) knob = 4095;
  const uint64_t eased = (uint64_t)knob * knob * knob;
  const uint64_t maximum = (uint64_t)4095u * 4095u * 4095u;
  return REALTIME_STRETCH_Q8_ONE +
         (uint32_t)((eased *
                         (REALTIME_STRETCH_Q8_MAX - REALTIME_STRETCH_Q8_ONE) +
                     maximum / 2) /
                    maximum);
}
void set_realtime_stretch_q8(uint32_t value) {
  if (value < REALTIME_STRETCH_Q8_ONE) value = REALTIME_STRETCH_Q8_ONE;
  if (value > REALTIME_STRETCH_Q8_MAX) value = REALTIME_STRETCH_Q8_MAX;
  realtime_stretch_q8 = value;
}
void set_realtime_stretch_knob(uint16_t knob) {
  set_realtime_stretch_q8(realtime_stretch_from_knob_q8(knob));
}
uint32_t realtime_stretch_effective_q8(void) {
  uint32_t value = realtime_stretch_q8;
  if (sf->fx_active[FX_TIMESTRETCH] && value < REALTIME_STRETCH_Q8_EFFECT)
    value = REALTIME_STRETCH_Q8_EFFECT;
  return value;
}
bool realtime_stretch_is_active(void) { return realtime_stretch_active; }
bool realtime_stretch_target_is_active(void) {
  return realtime_stretch_effective_q8() >= REALTIME_STRETCH_Q8_BYPASS;
}
uint32_t realtime_stretch_channels(void) {
  return banks[sel_bank_cur]
             ->sample[sel_sample_cur]
             .snd[FILEZERO]
             ->num_channels +
         1u;
}
uint32_t realtime_stretch_bytes_per_frame(void) {
  return realtime_stretch_channels() * 2u;
}
uint32_t realtime_stretch_frame_count(void) {
  return banks[sel_bank_cur]->sample[sel_sample_cur].snd[FILEZERO]->size /
         realtime_stretch_bytes_per_frame();
}
uint32_t realtime_stretch_preroll_bytes(void) {
  SampleInfo* si = banks[sel_bank_cur]->sample[sel_sample_cur].snd[FILEZERO];
  return (uint32_t)(si->num_channels + 1) * (si->oversampling + 1) * 44100u;
}
static RealtimeStretchBounds realtime_stretch_bounds(void) {
  SampleInfo* si = banks[sel_bank_cur]->sample[sel_sample_cur].snd[FILEZERO];
  uint32_t bytes = realtime_stretch_bytes_per_frame(),
           count = realtime_stretch_frame_count();
  RealtimeStretchBounds b = {0, count, 0, count, false};
  if (si->play_mode == PLAY_NORMAL || !si->slice_num) return b;
  unsigned slice = si->slice_current % si->slice_num;
  uint32_t first =
      si->slice_start[slice] > 0 ? (uint32_t)si->slice_start[slice] / bytes : 0;
  uint32_t last =
      si->slice_stop[slice] > 0 ? (uint32_t)si->slice_stop[slice] / bytes : 0;
  if (first >= count || last <= first || last > count) return b;
  b.stop =
      si->play_mode == PLAY_SPLICE_STOP || si->play_mode == PLAY_SAMPLE_STOP;
  if (si->play_mode == PLAY_SPLICE_STOP || si->play_mode == PLAY_SPLICE_LOOP) {
    b.first = b.loop_first = first;
    b.last = b.loop_last = last;
  } else if (si->play_mode == PLAY_SAMPLE_LOOP) {
    // The first pass may start anywhere. Subsequent wraps return to the slice.
    b.loop_first = first;
    b.loop_last = last;
  }
  return b;
}
void realtime_stretch_invalidate_grains(void) {
  realtime_stretch_grains_initialized = false;
}
void realtime_stretch_sync_phase_to_playback(void) {
  phases[0] = (int32_t)((realtime_stretch_phase_q32 >> 32u) *
                        realtime_stretch_bytes_per_frame());
  phases[1] = phases[0];
  // Do not clear phase_change: a newly published trigger still needs consuming.
}
void realtime_stretch_reset_from_playback_phase(void) {
  RealtimeStretchBounds b = realtime_stretch_bounds();
  uint32_t frame =
      phases[0] > 0 ? (uint32_t)phases[0] / realtime_stretch_bytes_per_frame()
                    : 0;
  if (b.last > b.first) {
    if (frame >= b.last) frame = phase_forward ? b.loop_first : b.last - 1;
    if (frame < b.first) frame = b.first;
  } else
    frame = 0;
  realtime_stretch_phase_q32 = (uint64_t)frame << 32u;
  realtime_stretch_invalidate_grains();
  realtime_stretch_begin_transition();
}
void realtime_stretch_update_state(void) {
  const uint32_t effective = realtime_stretch_effective_q8();
  const bool active = effective >= REALTIME_STRETCH_Q8_BYPASS;
  if (active != realtime_stretch_active) {
    if (active)
      realtime_stretch_reset_from_playback_phase();
    else {
      realtime_stretch_sync_phase_to_playback();
      realtime_stretch_invalidate_grains();
      realtime_stretch_begin_transition();
    }
  }
  realtime_stretch_active = active;
  realtime_stretch_applied_q8 = active ? effective : REALTIME_STRETCH_Q8_ONE;
}
void realtime_stretch_initialize_grains(uint64_t increment) {
  const int64_t offset = (int64_t)(REALTIME_STRETCH_GRAIN_HOP * increment);
  const int64_t phase = (int64_t)realtime_stretch_phase_q32;
  realtime_stretch_forward = phase_forward;
  realtime_stretch_grains[0] = (RealtimeStretchGrain){phase, increment, 0};
  realtime_stretch_grains[1] =
      (RealtimeStretchGrain){phase + (phase_forward ? -offset : offset),
                             increment, REALTIME_STRETCH_GRAIN_HOP};
  realtime_stretch_grains_initialized = true;
}
uint32_t realtime_stretch_grain_window(uint16_t age) {
  if (age >= REALTIME_STRETCH_GRAIN_LENGTH) return 0;
  return age <= REALTIME_STRETCH_GRAIN_HOP
             ? age
             : REALTIME_STRETCH_GRAIN_LENGTH - age;
}
// Read contiguous runs, reversing frames (not channels) in memory. Grain reads
// outside a stop region are silent; loop reads wrap without ever opening a
// file.
static bool realtime_stretch_read_frames(int64_t frame, uint32_t count,
                                         int16_t* dst,
                                         RealtimeStretchBounds b) {
  uint32_t channels = realtime_stretch_channels(), done = 0;
  if (b.last <= b.first) return false;
  while (done < count) {
    if (frame < b.first || frame >= b.last) {
      if (b.stop) {
        // Skip a whole out-of-range run rather than one frame/SD operation.
        uint64_t gap = frame < b.first ? (uint64_t)(b.first - frame)
                                       : (uint64_t)(frame - b.last + 1);
        uint32_t n = count - done;
        bool approaching = (phase_forward && frame < b.first) ||
                           (!phase_forward && frame >= b.last);
        if (approaching && gap < n) n = (uint32_t)gap;
        for (uint32_t i = 0; i < n * channels; ++i)
          dst[done * channels + i] = 0;
        done += n;
        frame += phase_forward ? (int64_t)n : -(int64_t)n;
        continue;
      }
      if (frame < b.first)
        frame =
            b.loop_last - 1 - ((b.first - 1 - frame) % (b.loop_last - b.first));
      else
        frame = b.loop_first + ((frame - b.last) % (b.last - b.loop_first));
    }
    uint32_t n = phase_forward ? b.last - (uint32_t)frame
                               : (uint32_t)frame - b.first + 1;
    if (n > count - done) n = count - done;
    uint32_t first = phase_forward ? (uint32_t)frame : (uint32_t)frame - n + 1;
    uint32_t offset =
        WAV_HEADER + realtime_stretch_preroll_bytes() + first * channels * 2u;
    FRESULT result = zd_f_lseek(&fil_current, offset, ZD_STRETCH_SEEK);
    unsigned bytes_read = 0;
    if (result == FR_OK)
      result = zd_f_read(&fil_current, dst + done * channels, n * channels * 2u,
                         &bytes_read, ZD_STRETCH_READ);
    if (result != FR_OK) {
      audio_media_io_failed(result);
      return false;
    }
    for (uint32_t i = bytes_read / 2u; i < n * channels; ++i)
      dst[done * channels + i] = 0;
    if (!phase_forward)
      for (uint32_t i = 0; i < n / 2; ++i)
        for (uint32_t c = 0; c < channels; ++c) {
          int16_t tmp = dst[(done + i) * channels + c];
          dst[(done + i) * channels + c] =
              dst[(done + n - 1 - i) * channels + c];
          dst[(done + n - 1 - i) * channels + c] = tmp;
        }
    done += n;
    frame += phase_forward ? (int64_t)n : -(int64_t)n;
  }
  return true;
}
static int16_t realtime_stretch_interpolated_frame(const int16_t* src,
                                                   uint32_t frame,
                                                   uint32_t channel,
                                                   uint32_t channels,
                                                   uint32_t frac) {
  int32_t a = src[frame * channels + channel],
          diff = src[(frame + 1) * channels + channel] - a;
  return (int16_t)(a + (int32_t)(((int64_t)diff * frac) >> 32u));
}
static void realtime_stretch_advance(uint64_t amount, RealtimeStretchBounds b) {
  uint64_t first = (uint64_t)b.first << 32u, last = (uint64_t)b.last << 32u;
  if (phase_forward) {
    realtime_stretch_phase_q32 += amount;
    if (realtime_stretch_phase_q32 >= last) {
      if (b.stop) {
        realtime_stretch_phase_q32 = last;
        mute_because_of_playback_type = true;
      } else {
        uint64_t loop = (uint64_t)b.loop_first << 32u;
        realtime_stretch_phase_q32 =
            loop + (realtime_stretch_phase_q32 - last) % (last - loop);
      }
    }
  } else if (amount > realtime_stretch_phase_q32 - first) {
    if (b.stop) {
      realtime_stretch_phase_q32 = first;
      mute_because_of_playback_type = true;
    } else {
      uint64_t loop = (uint64_t)b.loop_last << 32u;
      uint64_t overshoot = amount - (realtime_stretch_phase_q32 - first);
      realtime_stretch_phase_q32 = loop - 1 - (overshoot - 1) % (loop - first);
    }
  } else
    realtime_stretch_phase_q32 -= amount;
}
bool realtime_stretch_render(int16_t* values, uint32_t count,
                             uint64_t increment) {
  if (!increment) increment = REALTIME_STRETCH_PHASE_INC_Q32;
  if (increment > (4ull << 32u)) increment = 4ull << 32u;
  RealtimeStretchBounds bounds = realtime_stretch_bounds();
  for (uint32_t i = 0; i < count * 2; ++i) values[i] = 0;
  if (bounds.last <= bounds.first) return false;
  if (realtime_stretch_grains_initialized &&
      realtime_stretch_forward != phase_forward) {
    realtime_stretch_invalidate_grains();
    realtime_stretch_begin_transition();
  }
  if (!realtime_stretch_grains_initialized)
    realtime_stretch_initialize_grains(increment);
  const uint64_t transport_step =
      (increment << 8u) / realtime_stretch_applied_q8;
  const uint32_t channels = realtime_stretch_channels();
  uint32_t rendered = 0;
  while (rendered < count && !mute_because_of_playback_type) {
    uint32_t segment = count - rendered;
    if (segment > SAMPLES_PER_BUFFER) segment = SAMPLES_PER_BUFFER;
    for (unsigned g = 0; g < 2; ++g) {
      uint32_t remaining =
          REALTIME_STRETCH_GRAIN_LENGTH - realtime_stretch_grains[g].age;
      if (segment > remaining) segment = remaining;
    }
    uint64_t until_stop = UINT64_MAX;
    if (bounds.stop) {
      uint64_t distance =
          phase_forward
              ? ((uint64_t)bounds.last << 32u) - realtime_stretch_phase_q32
              : realtime_stretch_phase_q32 - ((uint64_t)bounds.first << 32u) +
                    1;
      until_stop = (distance + transport_step - 1) / transport_step;
      if (segment > until_stop) segment = (uint32_t)until_stop;
      if (!segment) {
        mute_because_of_playback_type = true;
        break;
      }
    }
    for (unsigned g = 0; g < 2; ++g) {
      RealtimeStretchGrain* grain = &realtime_stretch_grains[g];
      int64_t travelled = (int64_t)(grain->age * grain->phase_inc_q32);
      int64_t start =
          grain->start_phase_q32 + (phase_forward ? travelled : -travelled);
      int64_t frame =
          phase_forward ? start >> 32u : (start + 0xffffffffll) >> 32u;
      uint32_t fraction = phase_forward
                              ? (uint32_t)start
                              : (uint32_t)(frame * 4294967296ll - start);
      uint64_t span = fraction + (segment - 1) * grain->phase_inc_q32;
      uint32_t to_read = (uint32_t)(span >> 32u) + 2u;
      if (to_read > REALTIME_STRETCH_MAX_SOURCE_FRAMES ||
          !realtime_stretch_read_frames(frame, to_read,
                                        realtime_stretch_readbuf, bounds))
        return false;
      for (uint32_t i = 0; i < segment; ++i) {
        uint32_t weight = realtime_stretch_grain_window(grain->age + i);
        uint64_t local = fraction + i * grain->phase_inc_q32;
        for (unsigned c = 0; c < 2; ++c) {
          unsigned output = (rendered + i) * 2 + c;
          int32_t contribution =
              realtime_stretch_interpolated_frame(
                  realtime_stretch_readbuf, (uint32_t)(local >> 32u),
                  channels == 1 ? 0 : c, channels, (uint32_t)local) *
              (int32_t)weight;
          values[output] =
              (int16_t)(((int32_t)values[output] * 1024 + contribution) >>
                        REALTIME_STRETCH_GRAIN_HOP_SHIFT);
        }
      }
    }
    if (bounds.stop)
      for (uint32_t i = 0; i < segment; ++i) {
        uint64_t left = until_stop - i;
        if (left < REALTIME_STRETCH_TRANSITION_FRAMES)
          for (unsigned c = 0; c < 2; ++c)
            values[(rendered + i) * 2 + c] =
                (int16_t)((int32_t)values[(rendered + i) * 2 + c] *
                          (int32_t)left / REALTIME_STRETCH_TRANSITION_FRAMES);
      }
    realtime_stretch_advance(segment * transport_step, bounds);
    for (unsigned g = 0; g < 2; ++g) {
      RealtimeStretchGrain* grain = &realtime_stretch_grains[g];
      grain->age += segment;
      if (grain->age == REALTIME_STRETCH_GRAIN_LENGTH)
        *grain = (RealtimeStretchGrain){(int64_t)realtime_stretch_phase_q32,
                                        increment, 0};
    }
    rendered += segment;
  }
  realtime_stretch_sync_phase_to_playback();
  return true;
}
#endif
