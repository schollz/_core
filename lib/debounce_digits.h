#ifndef LIB_DEBOUNCEDIGITS_H
#define LIB_DEBOUNCEDIGITS_H 1

typedef struct DebounceDigits {
  uint16_t duration;
  uint16_t max_duration;
  uint16_t value : 9;
  uint16_t active : 1;
  uint16_t num_digits : 2;
  uint16_t current_digit : 2;
  uint16_t repeats : 2;
  char digits[3];
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

void DebounceDigits_set(DebounceDigits *self, uint16_t number,
                        uint16_t duration) {
  self->value = number;
  self->max_duration = duration;
  self->duration = duration;
  self->current_digit = 0;
  self->repeats = 0;
  self->active = 1;
  // determine the digits
  int i = 0;
  if (number == 0) {
    self->digits[i++] = '0';
  } else {
    while (number > 0) {
      self->digits[i++] =
          (number % 10) + '0';  // Convert digit to char and store in array
      number /= 10;
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
  // printf("self->duration: %d\n", self->duration);
  if (self->duration == 0) {
    self->current_digit++;
    // printf("current digit: %d/%d\n", self->current_digit, self->num_digits);
    if (self->current_digit == self->num_digits) {
      if (++self->repeats == 3) {
        self->active = 0;
        return false;
      }
      self->current_digit = 0;
    }
    self->duration = self->max_duration;
  }
  return true;
}

char DebounceDigits_get(DebounceDigits *self) {
  return self->digits[self->current_digit];
}

#endif