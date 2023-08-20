//  gcc -o main main.c && ./main  | gnuplot -p -e 'plot "/dev/stdin"  using 0:1
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define NOSDCARD

#include "../../pcg_basic.h"
#include "../../savefile.h"

int main() {
  SaveFile *sf;
  sf = SaveFile_New();

  printf("bpm: %d\n", sf->bpm_tempo);

  pcg32_random_t rng;
  pcg32_srandom_r(&rng, 42u, 54u);

  SaveFile_PatternRandom(sf, &rng, 0, 8);
  SaveFile_PatternPrint(sf);
  SaveFile_PatternRandom(sf, &rng, 0, 8);
  SaveFile_PatternPrint(sf);
  return 0;
}