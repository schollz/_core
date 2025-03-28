// Copyright 2023-2025 Zack Scholl, GPLv3.0

void run_keyboard() {
  int c = getchar_timeout_us(100);
  if (c >= 0) {
    printf("Got character %c\n", c);
  }
}