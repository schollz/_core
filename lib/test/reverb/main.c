// Copyright 2023-2025 Zack Scholl, GPLv3.0

//  gcc -o main main.c && ./main  | gnuplot -p -e 'plot "/dev/stdin"  using 0:1
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static uint32_t available_heap;
uint32_t getFreeHeap() { return available_heap; }

#define __not_in_flash_func(name) name

#include "../../freeverb_fp.h"

int main(void) {
  const uint32_t allocation = (uint32_t)FV_Reverb_heap_size(
      FV_NUMCOMBS_DEFAULT, FV_NUMALLPASSES_DEFAULT);
  available_heap = allocation + FV_REVERB_HEAP_RESERVE - 1;
  FV_Reverb *freeverb = FV_Reverb_malloc(FV_INITIALROOM, FV_INITIALDAMP,
                                         FV_INITIALWET, FV_INITIALDRY);
  assert(freeverb == NULL);

  available_heap = allocation + FV_REVERB_HEAP_RESERVE;
  freeverb = FV_Reverb_malloc(FV_INITIALROOM, FV_INITIALDAMP, FV_INITIALWET,
                              FV_INITIALDRY);
  assert(freeverb != NULL);
  assert(freeverb->num_combs == 1);
  assert(freeverb->num_allpasses == 1);
  FV_Reverb_free(freeverb);

  freeverb = FV_Reverb_malloc(FV_INITIALROOM, FV_INITIALDAMP, FV_INITIALWET,
                              FV_INITIALDRY);
  assert(freeverb != NULL);
  FV_Reverb_free(freeverb);
  puts("bounded Freeverb tests passed");
  return 0;
}
