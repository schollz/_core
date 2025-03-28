// Copyright 2023-2025 Zack Scholl, GPLv3.0
#define IS_DESKTOP 1
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../debounce_digits.h"

int main() {
  DebounceDigits *dd = DebounceDigits_malloc();

  DebounceDigits_set(dd, 123, 4);
  for (int i = 0; i < 59; i++) {
    DebounceDigits_active(dd);
    printf("%d %c\n", i, DebounceDigits_get(dd));
  }

  DebounceDigits_free(dd);

  return 0;
}
