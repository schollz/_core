// Copyright 2023-2025 Zack Scholl, GPLv3.0

//  gcc -o main main.c && ./main  | gnuplot -p -e 'plot "/dev/stdin"  using 0:1
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define NOSDCARD

#include "../../sampleinfo.h"

int main() {
  // read from sampleinfo.bin
  SampleInfo *sampleinfo = SampleInfo_readFromDisk("sampleinfo.bin");
  // print out all the sample info variables
  printf("size: %d\n", sampleinfo->size);
  printf("bpm: %d\n", sampleinfo->bpm);
  printf("play_mode: %d\n", sampleinfo->play_mode);
  printf("one_shot: %d\n", sampleinfo->one_shot);
  printf("tempo_match: %d\n", sampleinfo->tempo_match);
  printf("oversampling: %d\n", sampleinfo->oversampling);
  printf("num_channels: %d\n", sampleinfo->num_channels);
  printf("version: %d\n", sampleinfo->version);
  printf("reserved: %d\n", sampleinfo->reserved);
  printf("splice_trigger: %d\n", sampleinfo->splice_trigger);
  printf("splice_variable: %d\n", sampleinfo->splice_variable);
  printf("slice_num: %d\n", sampleinfo->slice_num);
  printf("slice_current: %d\n", sampleinfo->slice_current);
  for (int i = 0; i < sampleinfo->slice_num; i++) {
    printf("slice_start[%d]: %d\n", i, sampleinfo->slice_start[i]);
  }
  for (int i = 0; i < sampleinfo->slice_num; i++) {
    printf("slice_stop[%d]: %d\n", i, sampleinfo->slice_stop[i]);
  }
  for (int i = 0; i < sampleinfo->slice_num; i++) {
    printf("slice_type[%d]: %d\n", i, sampleinfo->slice_type[i]);
  }

  SampleInfo_free(sampleinfo);
  return 0;
}