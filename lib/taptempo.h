// Copyright 2023-2024 Zack Scholl.
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

#ifndef TAPTEMPO_LIB
#define TAPTEMPO_LIB 1
#define TAPTEMPO_SIZE 10
#define TAPTEMPO_MS_LIMIT 1000

#ifdef LINUX_SYSTEM
#include <sys/time.h>
#endif

typedef struct TapTempo {
  uint32_t last;
  uint16_t taps[TAPTEMPO_SIZE];
  uint8_t index;
#ifdef LINUX_SYSTEM
  struct timeval last_time;
#endif
} TapTempo;

TapTempo *TapTempo_malloc() {
  TapTempo *self = (TapTempo *)malloc(sizeof(TapTempo));
  self->last = 0;
  self->index = 0;
  for (int i = 0; i < 4; i++) {
    self->taps[i] = 0;
  }
#ifdef LINUX_SYSTEM
  gettimeofday(&self->last_time, NULL);
#else
  self->last = time_us_32();
#endif
  return self;
}

void TapTempo_free(TapTempo *self) { free(self); }

// TapTempo_tap returns the current tempo in bpm
// calculated from a weighted average of the last taps
uint16_t TapTempo_tap(TapTempo *self) {
  // change the time calculation based on whether its linux or arduino
  uint16_t milliseconds;
#ifdef LINUX_SYSTEM
  struct timeval current_time;
  gettimeofday(&current_time, NULL);
  milliseconds = ((current_time.tv_sec - self->last_time.tv_sec) * 1000000L +
                  (current_time.tv_usec - self->last_time.tv_usec)) /
                 1000;
  self->last_time = current_time;
#else
  uint32_t current_time = time_us_32();
  milliseconds = (current_time - self->last) / 1000;
  self->last = current_time;
//   printf("[taptempo] milliseconds %d\n", milliseconds);
#endif
  if (milliseconds > TAPTEMPO_MS_LIMIT) {
    for (int i = 0; i < TAPTEMPO_SIZE; i++) {
      self->taps[i] = 0;
    }
    return 0;
  }
  self->taps[self->index] = milliseconds;
  self->index = (self->index + 1) % TAPTEMPO_SIZE;
  uint32_t sum = 0;
  uint8_t count = 0;
  for (int i = 0; i < TAPTEMPO_SIZE; i++) {
    if (self->taps[i] > 0) {
      count++;
      sum += self->taps[i];
    }
  }
  milliseconds = round(60000.0 / ((float)sum / count));
  if (milliseconds > 300) {
    milliseconds = 300;
  } else if (milliseconds < 30) {
    milliseconds = 30;
  }
  return milliseconds;
}

#endif