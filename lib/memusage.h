#include <malloc.h>
uint32_t getTotalHeap() {
  extern char __StackLimit, __bss_end__;

  return &__StackLimit - &__bss_end__;
}

uint32_t getFreeHeap() {
  struct mallinfo m = mallinfo();

  return getTotalHeap() - m.uordblks;
}
