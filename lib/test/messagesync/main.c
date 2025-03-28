// Copyright 2023-2025 Zack Scholl, GPLv3.0
#include "../../messagesync.h"

int main() {
  MessageSync *ms = MessageSync_malloc();
  MessageSync_append(ms, "Hello, World!\n");
  MessageSync_append(ms, "Hello, again!\n");
  MessageSync_print(ms);
  MessageSync_clear(ms);
  MessageSync_print(ms);
  MessageSync_printf(ms, "%d + %d = %d\n", 1, 1, 1 + 1);
  MessageSync_printf(ms, "%d + %d = %d\n", 2, 1, 2 + 1);
  MessageSync_printf(ms, "%d + %d = %d\n", 4, 1, 4 + 1);
  MessageSync_printf(ms, "%d + %d = %d\n", 3, 1, 3 + 1);
  MessageSync_print(ms);
  MessageSync_free(ms);

  return 0;
}
