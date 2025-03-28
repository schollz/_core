#ifndef LIB_DEBOUNCE_H
#define LIB_DEBOUNCE_H 1

typedef struct DebounceUint8 {
  uint16_t duration;
  uint8_t value;
} DebounceUint8;

DebounceUint8 *DebounceUint8_malloc() {
  DebounceUint8 *self = (DebounceUint8 *)malloc(sizeof(DebounceUint8));
  if (self == NULL) {
    perror("Error allocating memory for struct");
    return NULL;
  }
  self->duration = 0;
  self->value = 0;
  return self;
}

void DebounceUint8_free(DebounceUint8 *self) {
  if (self == NULL) {
    return;
  }
  free(self);
}

void DebounceUint8_clear(DebounceUint8 *self) { self->duration = 0; }

void DebounceUint8_set(DebounceUint8 *self, uint8_t value, uint16_t duration) {
  self->duration = duration;
  self->value = value;
}

bool DebounceUint8_active(DebounceUint8 *self) {
  if (self->duration == 0) {
    return false;
  }
  self->duration--;
  if (is_arcade_box) {
    for (uint8_t i = 0; i < 3; i++) {
      if (self->duration == 0) {
        break;
      }
      self->duration--;
    }
  }
  return true;
}

uint8_t DebounceUint8_get(DebounceUint8 *self) { return self->value; }

#endif