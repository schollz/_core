
// Copyright 2023-2025 Zack Scholl, GPLv3.0

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