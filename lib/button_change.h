// Copyright 2023-2025 Zack Scholl, GPLv3.0

#ifndef BUTTON_CHANGE_LIB
#define BUTTON_CHANGE_LIB 1

typedef struct ButtonChange {
  int8_t last;
  uint8_t debounce;
} ButtonChange;

ButtonChange *ButtonChange_malloc() {
  ButtonChange *self = (ButtonChange *)malloc(sizeof(ButtonChange));
  self->last = -1;
  self->debounce = 0;
  return self;
}

void ButtonChange_free(ButtonChange *self) { free(self); }

int8_t ButtonChange_update(ButtonChange *self, int8_t val) {
  if (self->debounce > 0) {
    self->debounce--;
  }
  if (val != self->last) {
    self->last = val;
    if (val > 0 && self->debounce > 0) {
      return -1;
    }
    if (val > 0) {
      self->debounce = 255;
    }
    return val;
  }
  return -1;
}

#endif