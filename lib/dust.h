
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

#ifndef DUST_LIB
#define DUST_LIB 1

typedef struct Dust {
  uint32_t last_time;
  uint32_t next_firing;
  uint32_t duration : 31;  // in ms
  uint32_t active : 1;     // 0: inactive, 1: active
  callback_void emit;
} Dust;

Dust* Dust_malloc() {
  Dust* self = (Dust*)malloc(sizeof(Dust));
  self->last_time = 0;
  self->next_firing = 0;
  self->duration = 0;
  self->active = 0;
  self->emit = NULL;
  return self;
}

void Dust_update(Dust* self) {
  if (!self->active) {
    return;
  }
  uint32_t current_time = time_us_32();
  if (current_time > self->next_firing) {
    // printf("current_time: %d, next_firing: %d\n", current_time,
    //        self->next_firing);
    self->last_time = current_time;
    self->next_firing =
        self->last_time + random_integer_in_range(0, 2 * self->duration);
    if (self->emit != NULL) {
      self->emit();
    }
  }
}

// Dust_setDuration sets in milliseconds
void Dust_setDuration(Dust* self, uint32_t duration) {
  self->active = 1;
  self->duration = duration * 1000;
  self->last_time = time_us_32();
  self->next_firing =
      self->last_time + random_integer_in_range(0, 2 * self->duration);
}

// Dust_setFrequency sets in milliHz
void Dust_setFrequency(Dust* self, uint16_t frequency) {
  if (frequency < 20) {
    self->active = 0;
    return;
  }
  self->active = 1;
  self->duration = 1000000 / frequency;
  self->last_time = time_us_32();
  self->next_firing =
      self->last_time + random_integer_in_range(0, 2 * self->duration);
}

void Dust_setCallback(Dust* self, callback_void emit) { self->emit = emit; }

#endif