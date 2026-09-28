#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "sample_cv.h"

static int16_t adc_for_note(int note) {
  return (int16_t)lround(512.0 + note * 512.0 / 60.0);
}

int main(void) {
  SampleCVState state = {0};
  uint8_t sample = 99;
  assert(!sample_cv_index(&state, 8, &sample) && sample == 99);
  for (unsigned count = 1; count <= 16; ++count) {
    for (int polarity = 0; polarity <= 1; ++polarity) {
      // Exercise ascending, descending, and freshly acquired notes.
      for (int direction = -1; direction <= 1; ++direction) {
        sample_cv_reset(&state);
        for (int step = -60; step <= 60; ++step) {
          int note = direction < 0 ? -step : step;
          if (!direction) sample_cv_reset(&state);
          sample_cv_update(&state, adc_for_note(note), polarity);
          int expected = !polarity && note < 0 ? 0 : note;
          assert(state.note == expected);
          while (expected < 0) expected += count;
          assert(sample_cv_index(&state, count, &sample));
          assert(sample == expected % count);
        }
      }
    }
  }

  // A held octave maps using the new bank length, not the old sample ordinal.
  sample_cv_update(&state, adc_for_note(12), true);
  assert(sample_cv_index(&state, 8, &sample) && sample == 4);
  assert(sample_cv_index(&state, 5, &sample) && sample == 2);
  assert(sample_cv_index(&state, 16, &sample) && sample == 12);
  assert(!sample_cv_index(&state, 0, &sample) && sample == 12);

  // Nearest-note acquisition; hysteresis keeps boundary noise from chattering.
  sample_cv_reset(&state);
  sample_cv_update(&state, 512, true);
  for (int i = 0; i < 20; ++i) {
    sample_cv_update(&state, 516 + i % 2, true);
    assert(state.note == 0);
  }
  sample_cv_update(&state, 518, true);
  assert(state.note == 1);
  for (int i = 0; i < 20; ++i) {
    sample_cv_update(&state, 516 + i % 2, true);
    assert(state.note == 1);
  }
  sample_cv_update(&state, 515, true);
  assert(state.note == 0);
  sample_cv_update(&state, 506, true);
  assert(state.note == -1);
  sample_cv_update(&state, 508, true);
  assert(state.note == -1);
  sample_cv_reset(&state);  // Replug acquires the nearest note without history.
  sample_cv_update(&state, 508, true);
  assert(state.note == 0);

  sample_cv_update(&state, adc_for_note(-24), true);
  assert(state.note == -24);
  sample_cv_update(&state, adc_for_note(-24), false);
  assert(state.note == 0);
  sample_cv_update(&state, adc_for_note(-24), true);
  assert(state.note == -24);
  sample_cv_update(&state, 32767, true);
  assert(state.note == 60);
  sample_cv_update(&state, -32768, true);
  assert(state.note == -60);

  // Every nominal input stays within the bank, including its wrapping edges.
  for (int raw = 0; raw <= 1023; ++raw) {
    sample_cv_reset(&state);
    sample_cv_update(&state, raw, true);
    assert(sample_cv_index(&state, 7, &sample) && sample < 7);
    assert(sample_cv_index(&state, 1, &sample) && sample == 0);
  }
  puts("Sample CV quantization, polarity, wrapping, and hysteresis passed");
}
