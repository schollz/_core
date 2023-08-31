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

#ifndef BASS_LIB
#include "bass_raw.h"
#include "crossfade.h"

typedef struct Bass {
  uint32_t phase[2];
  int8_t phase_dir[2];
  uint8_t note[2];
  uint32_t phases_since_last[2];
} Bass;

Bass *Bass_create() {
  Bass *bass = (Bass *)malloc(sizeof(Bass));
  for (uint8_t i = 0; i < 2; i++) {
    bass->phase[i] = 0;
    bass->phase_dir[i] = 1;
    bass->phases_since_last[i] = 0;
    bass->note[i] = 0;
  }
  return bass;
}

void Bass_destroy(Bass *bass) { free(bass); }

void Bass_trig(Bass *bass) {
  bass->note[1] = bass->note[0];
  bass->phase[1] = bass->phase[0];
  bass->phase_dir[1] = bass->phase_dir[0];
  bass->phase[0] = 0;
  bass->phase_dir[0] = 1;
  bass->phases_since_last[0] = 0;
  bass->phases_since_last[1] = 0;
}

void Bass_callback(Bass *bass, int32_t *samples, uint32_t sample_count,
                   uint volmain) {
  for (uint8_t head = 0; head < 2; head++) {
    for (uint32_t i = 0; i < sample_count; i++) {
      uint vol = volmain;
      if (bass->phases_since_last[head] < CROSSFADE_MAX) {
        if (head == 0) {
          vol = volmain - crossfade_vol(volmain, bass->phases_since_last[head]);
        } else {
          vol = crossfade_vol(volmain, bass->phases_since_last[head]);
        }
        bass->phases_since_last[head]++;
      } else if (head == 1) {
        continue;
      }

      int32_t value0 = (vol * 2 * bass_raw[bass->phase[head]]) << 8u;
      value0 = value0 + (value0 >> 16u);
      samples[i * 2 + 0] = samples[i * 2 + 0] + value0;  // L
      samples[i * 2 + 1] = samples[i * 2 + 1] + value0;  // R
      // update the phase
      bass->phase[head] += bass->phase_dir[head];
      if (bass->phase[head] == BASS_RAW_LEN - 1) {
        bass->phase_dir[head] = -1;
      } else if (bass->phase[head] == 0) {
        bass->phase_dir[head] = 1;
      }
    }
  }
  return;
}

#endif /* BASS_LIB */
#define BASS_LIB 1