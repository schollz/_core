// Copyright 2023-2025 Zack Scholl, GPLv3.0

//  gcc -o main main.c && ./main  | gnuplot -p -e 'plot "/dev/stdin"  using 0:1
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
int getFreeHeap() { return 20000; }

#include "../../freeverb_fp.h"

int main() {
  FV_Reverb *freeverb = NULL;
  printf("%d\n", freeverb == NULL);
  freeverb = FV_Reverb_malloc(FV_INITIALROOM, FV_INITIALDAMP, FV_INITIALWET,
                              FV_INITIALDRY);
  printf("%d\n", freeverb == NULL);
  FV_Reverb_free(freeverb);
  printf("%d\n", freeverb == NULL);
  freeverb = FV_Reverb_malloc(FV_INITIALROOM, FV_INITIALDAMP, FV_INITIALWET,
                              FV_INITIALDRY);
  printf("%d\n", freeverb == NULL);
  FV_Reverb_free(freeverb);
  return 0;
}