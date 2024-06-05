#ifndef LIB_DEBOUNCEDIGITS_H
#define LIB_DEBOUNCEDIGITS_H 1

typedef struct DebounceDigits {
  uint16_t duration;
  uint16_t max_duration;
  uint16_t value : 10;
  uint16_t active : 6;
  uint16_t num_digits : 6;
  uint16_t current_digit : 6;
  uint16_t repeats : 3;
  uint16_t space : 1;
  char digits[16];
} DebounceDigits;

DebounceDigits *DebounceDigits_malloc() {
  DebounceDigits *self = (DebounceDigits *)malloc(sizeof(DebounceDigits));
  if (self == NULL) {
    perror("Error allocating memory for struct");
    return NULL;
  }
  self->duration = 0;
  self->value = 0;
  self->active = 0;
  self->num_digits = 1;
  self->current_digit = 0;
  self->repeats = 0;
  self->digits[0] = '0';
  self->digits[1] = '\0';
  return self;
}

void DebounceDigits_free(DebounceDigits *self) {
  if (self == NULL) {
    return;
  }
  free(self);
}

void DebounceDigits_reverseArray(char *array, int length) {
  int start = 0;
  int end = length - 1;
  while (start < end) {
    char temp = array[start];
    array[start] = array[end];
    array[end] = temp;
    start++;
    end--;
  }
}

void DebounceDigits_setText(DebounceDigits *self, char *text,
                            uint16_t duration) {
  self->value = 0;
  self->max_duration = duration;
  self->duration = duration * 0.1;
  self->current_digit = 0;
  self->repeats = 0;
  self->active = 1;
  self->space = 1;
  self->num_digits = strlen(text);
  if (self->num_digits > 15) {
    self->num_digits = 15;
  }
  for (int i = 0; i < self->num_digits; i++) {
    self->digits[i] = text[i];
  }
  self->digits[self->num_digits] = '\0';
}

void DebounceDigits_set(DebounceDigits *self, uint16_t number,
                        uint16_t duration) {
  self->value = number;
  self->max_duration = duration;
  self->duration = duration * 0.1;
  self->current_digit = 0;
  self->repeats = 0;
  self->active = 1;
  self->space = 1;
  // determine the digits
  int i = 0;
  if (number == 0) {
    self->digits[i++] = '0';
  } else {
    uint8_t j = 0;
    while (number > 0 && j < 3) {
      self->digits[i++] =
          (number % 10) + '0';  // Convert digit to char and store in array
      number /= 10;
      j++;
    }
  }
  self->num_digits = i;
  self->digits[i] = '\0';  // Null-terminate the string
  DebounceDigits_reverseArray(self->digits,
                              i);  // Reverse the array to get correct order
}

bool DebounceDigits_active(DebounceDigits *self) {
  if (self->active == 0) {
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
  // printf("self->duration: %d\n", self->duration);
  if (self->duration == 0) {
    self->space = !self->space;
    if (self->space) {
      self->current_digit++;
      self->duration = self->max_duration * 0.1;
    } else {
      self->duration = self->max_duration;
    }
    // printf("current digit: %d/%d\n", self->current_digit, self->num_digits);
    if (self->current_digit == self->num_digits + 1) {
      if (++self->repeats == 3) {
        self->active = 0;
        return false;
      }
      self->current_digit = 0;
    }
  }
  return true;
}

void DebounceDigits_clear(DebounceDigits *self) { self->active = 0; }

char DebounceDigits_get(DebounceDigits *self) {
  if (self->active == 0) {
    return ' ';
  }
  if (self->current_digit == 0 || self->space) {
    return ' ';
  }
  return self->digits[self->current_digit - 1];
}

#endif