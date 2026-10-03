// Exercise the actual granular renderer against a bounded primary-only PCM
// reader.
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef SAMPLES_PER_BUFFER
#define SAMPLES_PER_BUFFER 441
#endif
#define FILEZERO 0
#define FX_TIMESTRETCH 4
#define PLAY_NORMAL 0
#define PLAY_SPLICE_STOP 1
#define PLAY_SPLICE_LOOP 2
#define PLAY_SAMPLE_STOP 3
#define PLAY_SAMPLE_LOOP 4
#define WAV_HEADER 44
#define ZD_STRETCH_SEEK 0
#define ZD_STRETCH_READ 0
#define FR_OK 0
typedef int FRESULT;
typedef struct {
  uint32_t size;
  unsigned num_channels, oversampling, slice_num, slice_current, play_mode;
  int32_t *slice_start, *slice_stop;
} SampleInfo;
static int32_t starts[1], stops[1];
static SampleInfo info;
static struct {
  struct {
    SampleInfo* snd[1];
  } sample[1];
} bank;
static __typeof__(&bank) banks[1] = {&bank};
static unsigned sel_bank_cur, sel_sample_cur;
static struct {
  bool fx_active[16];
} settings, *sf = &settings;
static int32_t phases[2], phase_new;
static bool phase_change, phase_forward = true, mute_because_of_playback_type;
static int fil_current;
static uint32_t read_offset, read_calls;
static int failed_io;
static bool tone;
static int16_t source(uint32_t frame, unsigned channel) {
  if (tone) return (int16_t)(12000 * sin(6.283185307179586 * frame / 64.));
  return (int16_t)((frame * 31u + channel * 7001u) % 24000u - 12000);
}
static int zd_f_lseek(int* f, uint32_t offset, int tag) {
  (void)f;
  (void)tag;
  read_offset = offset;
  return failed_io;
}
static int zd_f_read(int* f, void* out, uint32_t n, unsigned* read, int tag) {
  (void)f;
  (void)tag;
  uint32_t channels = info.num_channels + 1, bytes = channels * 2;
  uint32_t head = 44 + channels * (info.oversampling + 1) * 44100;
  assert(read_offset >= head && read_offset - head + n <= info.size &&
         n % bytes == 0);
  uint32_t frame = (read_offset - head) / bytes;
  for (unsigned i = 0; i < n / bytes; ++i)
    for (unsigned c = 0; c < channels; ++c)
      ((int16_t*)out)[i * channels + c] = source(frame + i, c);
  *read = n;
  ++read_calls;
  return failed_io;
}
static void audio_media_io_failed(int result) { assert(result == failed_io); }
#include "realtime_stretch.h"
static void consume_phase(void) {
  bool do_crossfade = false;
#define CL_CALL(...)
#define AR_CALL(...)
#include "consume_phase.h"
#undef CL_CALL
#undef AR_CALL
  assert(do_crossfade && !phase_change);
}
static void setup(unsigned frames, unsigned channels, unsigned rate,
                  unsigned mode, bool forward) {
  info = (SampleInfo){
      frames * channels * 2, channels - 1, rate - 1, 1, 0, mode, starts, stops};
  starts[0] = 0;
  stops[0] = info.size;
  bank.sample[0].snd[0] = &info;
  memset(&settings, 0, sizeof settings);
  phases[0] = phases[1] = 0;
  phase_forward = forward;
  phase_change = false;
  mute_because_of_playback_type = false;
  realtime_stretch_active = false;
  realtime_stretch_grains_initialized = false;
  failed_io = 0;
  read_calls = 0;
  tone = false;
  set_realtime_stretch_q8(8 * 256);
  realtime_stretch_update_state();
}
static void controls(void) {
  setup(65536, 2, 1, PLAY_NORMAL, true);
  uint32_t previous = 0;
  for (unsigned k = 0; k <= 4095; ++k) {
    uint32_t value = realtime_stretch_from_knob_q8(k);
    assert(value >= previous && value <= 4096);
    previous = value;
  }
  assert(realtime_stretch_from_knob_q8(0) == 256 && previous == 4096);
  assert(realtime_stretch_from_knob_q8(UINT16_MAX) == 4096);
  set_realtime_stretch_q8(UINT32_MAX);
  assert(realtime_stretch_q8 == 4096);
  set_realtime_stretch_q8(0);
  assert(realtime_stretch_q8 == 256);
  for (unsigned manual = 256; manual <= 4096; manual += 256) {
    set_realtime_stretch_q8(manual);
    sf->fx_active[4] = true;
    realtime_stretch_update_state();
    assert(realtime_stretch_applied_q8 == (manual < 2048 ? 2048 : manual));
    sf->fx_active[4] = false;
    realtime_stretch_update_state();
    assert(realtime_stretch_q8 == manual);
    assert(realtime_stretch_applied_q8 == manual);
  }
  set_realtime_stretch_q8(281);
  realtime_stretch_update_state();
  assert(!realtime_stretch_is_active());
  set_realtime_stretch_q8(282);
  realtime_stretch_update_state();
  assert(realtime_stretch_is_active());
  phase_change = true;
  set_realtime_stretch_q8(256);
  realtime_stretch_update_state();
  assert(phase_change);  // Bypassing must not consume an incoming trigger.
}
static void progression(void) {
  int16_t out[SAMPLES_PER_BUFFER * 2];
  for (unsigned channels = 1; channels <= 2; ++channels)
    for (unsigned rate = 1; rate <= 2; ++rate)
      for (unsigned reverse = 0; reverse < 2; ++reverse)
        for (unsigned ratio = 8; ratio <= 16; ratio += 2) {
          setup(65536, channels, rate, PLAY_NORMAL, !reverse);
          phases[0] = 32768 * channels * 2;
          realtime_stretch_reset_from_playback_phase();
          set_realtime_stretch_q8(ratio * 256);
          realtime_stretch_update_state();
          uint64_t initial = realtime_stretch_phase_q32,
                   inc = (uint64_t)rate << 32u;
          for (unsigned block = 0; block < 32; ++block)
            assert(realtime_stretch_render(out, SAMPLES_PER_BUFFER, inc));
          uint64_t delta = (inc / ratio) * SAMPLES_PER_BUFFER * 32;
          assert(realtime_stretch_phase_q32 ==
                 (reverse ? initial - delta : initial + delta));
          assert(read_calls > 0);
          if (channels == 1)
            for (unsigned i = 0; i < SAMPLES_PER_BUFFER; ++i)
              assert(out[2 * i] == out[2 * i + 1]);
          uint64_t position = realtime_stretch_phase_q32;
          sf->fx_active[4] = true;
          realtime_stretch_update_state();
          sf->fx_active[4] = false;
          realtime_stretch_update_state();
          assert(realtime_stretch_phase_q32 == position);
          // A direction change reverses both the source scan and grain
          // playback.
          phase_forward = !phase_forward;
          assert(realtime_stretch_render(out, SAMPLES_PER_BUFFER, inc));
          assert(realtime_stretch_phase_q32 ==
                 (reverse ? position + (inc / ratio) * SAMPLES_PER_BUFFER
                          : position - (inc / ratio) * SAMPLES_PER_BUFFER));
        }
}
static void boundaries(void) {
  int16_t out[SAMPLES_PER_BUFFER * 2];
  for (unsigned mode = 0; mode <= 4; ++mode)
    for (unsigned reverse = 0; reverse < 2; ++reverse) {
      setup(32, 2, 1, mode, !reverse);
      starts[0] = 8 * 4;
      stops[0] = 24 * 4;
      phases[0] = (reverse ? 24 : 8) * 4;
      realtime_stretch_reset_from_playback_phase();
      for (unsigned block = 0; block < 12; ++block)
        assert(realtime_stretch_render(out, SAMPLES_PER_BUFFER, 1ull << 32));
      bool stop = mode == PLAY_SPLICE_STOP || mode == PLAY_SAMPLE_STOP;
      assert(mute_because_of_playback_type == stop);
      if (stop) {
        unsigned expected = reverse ? (mode == PLAY_SPLICE_STOP ? 8 : 0)
                                    : (mode == PLAY_SPLICE_STOP ? 24 : 32);
        assert(phases[0] == (int32_t)expected * 4);
        for (unsigned i = 0; i < SAMPLES_PER_BUFFER * 2; ++i)
          assert(out[i] == 0);
      } else {
        uint32_t f = (uint32_t)(realtime_stretch_phase_q32 >> 32);
        assert(f < 32);
        if (mode == PLAY_SPLICE_LOOP) assert(f >= 8 && f < 24);
        if (mode == PLAY_SAMPLE_LOOP) assert(reverse ? f < 24 : f >= 8);
      }
    }
  // Directional reads preserve stereo channels and interpolation order.
  setup(32, 2, 1, PLAY_SPLICE_LOOP, false);
  starts[0] = 8 * 4;
  stops[0] = 24 * 4;
  RealtimeStretchBounds b = realtime_stretch_bounds();
  assert(realtime_stretch_read_frames(9, 5, out, b));
  unsigned frames[] = {9, 8, 23, 22, 21};
  for (unsigned i = 0; i < 5; ++i)
    for (unsigned c = 0; c < 2; ++c)
      assert(out[2 * i + c] == source(frames[i], c));
  setup(1, 1, 1, PLAY_NORMAL, true);
  assert(realtime_stretch_render(out, SAMPLES_PER_BUFFER, 4ull << 32));
  setup(65536, 2, 1, PLAY_NORMAL, true);
  failed_io = 1;
  assert(!realtime_stretch_render(out, SAMPLES_PER_BUFFER, 1ull << 32));
}
static void pitch_and_transition(void) {
  int16_t out[SAMPLES_PER_BUFFER * 2];
  for (unsigned ratio = 8; ratio <= 16; ratio += 8) {
    setup(65536, 2, 1, PLAY_NORMAL, true);
    tone = true;
    set_realtime_stretch_q8(ratio * 256);
    realtime_stretch_update_state();
    unsigned crossings = 0, total = 0;
    int16_t last = 0;
    for (unsigned b = 0; b < 100; ++b) {
      assert(realtime_stretch_render(out, SAMPLES_PER_BUFFER, 1ull << 32));
      for (unsigned i = 0; i < SAMPLES_PER_BUFFER; ++i) {
        if (last <= 0 && out[2 * i] > 0) ++crossings;
        last = out[2 * i];
        ++total;
      }
    }
    assert(abs((int)crossings - (int)(total / 64)) <= 2);
  }
  int32_t values[128];
  for (unsigned i = 0; i < 128; ++i) values[i] = 100000;
  realtime_stretch_transition_remaining = 0;
  realtime_stretch_declick(values, 64);
  realtime_stretch_begin_transition();
  for (unsigned i = 0; i < 128; ++i) values[i] = -100000;
  realtime_stretch_declick(values, 64);
  assert(values[0] > 90000 && values[126] == -100000);
  for (unsigned i = 2; i < 128; i += 2)
    assert(values[i - 2] - values[i] <= 3125);
}
static void trigger_reset(void) {
  int16_t out[SAMPLES_PER_BUFFER * 2];
  setup(65536, 2, 1, PLAY_NORMAL, true);
  set_realtime_stretch_q8(16 * 256);
  realtime_stretch_update_state();
  assert(realtime_stretch_render(out, SAMPLES_PER_BUFFER, 1ull << 32));
  assert(realtime_stretch_grains_initialized);
  phase_change = true;
  phase_new = 32768 * 4;
  consume_phase();
  assert(realtime_stretch_phase_q32 == 32768ull << 32);
  assert(!realtime_stretch_grains_initialized);
  assert(realtime_stretch_render(out, SAMPLES_PER_BUFFER, 1ull << 32));
  assert(realtime_stretch_phase_q32 ==
         (32768ull << 32) + (1ull << 28) * SAMPLES_PER_BUFFER);
  // A trigger arriving at the same block as bypass still wins over the old
  // granular transport position and is applied by the common callback.
  phase_change = true;
  phase_new = 16384 * 4;
  set_realtime_stretch_q8(256);
  realtime_stretch_update_state();
  consume_phase();
  assert(phases[0] == phase_new && !realtime_stretch_is_active());
}
int main(void) {
  controls();
  progression();
  boundaries();
  pitch_and_transition();
  trigger_reset();
  puts(
      "granular: 16x cap, effect composition, fractional phase, reverse, "
      "boundaries, channels, pitch, IO and transitions passed");
}
