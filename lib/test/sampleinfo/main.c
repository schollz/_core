#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "../../sampleinfo.h"

void basic_test() {
  SampleInfo *si;
  int32_t slice_start[3] = {0, 100, 200};
  int32_t slice_stop[3] = {100, 200, 300};
  int8_t slice_type[3] = {0, 1, 2};
  si = SampleInfo_malloc(100, 120, 0, 0, 0, 0, 0, 0, 1, 3, slice_start,
                         slice_stop, slice_type);

  SampleInfoPack *sp = SampleInfo_Marshal(si);
  SampleInfo_free(si);

  si = SampleInfo_Unmarshal(sp);
  SampleInfoPack_free(sp);

  printf("si->size: %d\n", si->size);
  for (int i = 0; i < si->slice_num; i++) {
    printf("si->slice_start[%d]: %d\n", i, si->slice_start[i]);
    printf("si->slice_stop[%d]: %d\n", i, si->slice_stop[i]);
    printf("si->slice_type[%d]: %d\n", i, si->slice_type[i]);
  }

  SampleInfo_free(si);
}
// Main function to test the above implementation

void read_file_test() {
  SampleInfoPack *sp = SampleInfoPack_readFromFile("go/sampleinfo.bin");
  SampleInfo *si = SampleInfo_Unmarshal(sp);
  SampleInfoPack_free(sp);
  printf("si->size: %d\n", si->size);
  for (int i = 0; i < si->slice_num; i++) {
    printf("si->slice_start[%d]: %d\n", i, si->slice_start[i]);
    printf("si->slice_stop[%d]: %d\n", i, si->slice_stop[i]);
    printf("si->slice_type[%d]: %d\n", i, si->slice_type[i]);
  }
  SampleInfo_free(si);
}

int main() {
  read_file_test();
  return 0;
}
