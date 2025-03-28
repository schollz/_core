// Copyright 2023-2025 Zack Scholl, GPLv3.0

//  gcc -o main main.c && ./main  | gnuplot -p -e 'plot "/dev/stdin"  using 0:1
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define NOSDCARD 1

#include "../../definitions.h"
#include "../../savefile.h"

int main() {
  SaveFile *sf;
  sf = SaveFile_malloc();

  SaveFile_load(sf, 0);
  printf("bpm: %d\n", sf->bpm_tempo);

  SaveFile_free(sf);
  return 0;
}