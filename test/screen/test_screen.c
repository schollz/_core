#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "golden.h"
#include "../bank_metadata/ff.h"
#include "../../lib/font.h"
#include "../../lib/zeptocore_screen.c"

uint32_t screen_test_now;
SampleList *banks[16];
static SampleInfo infos[16];
static Sample samples[16];
static SampleList bank;
static uint8_t file_data[16][SW_FILE_BYTES];
static int file_size[16], failed_read = -1, failed_close = -1;
static unsigned operations, handles, generation = 1, resident;
static uint8_t image[SHARP_MEMORY_BUFFER_BYTES];
static uint8_t workspace[512];
uint8_t *metadata_foreground_workspace(void) { return workspace; }
static int32_t starts[2] = {0,4}, stops[2] = {4,8};
bool metadata_ready(unsigned b) { return b == resident; }
unsigned metadata_generation(void) { return generation; }
int metadata_ordinal(unsigned b, unsigned slot) {
  return b < 16 && banks[b] && slot < 16 && (bank.valid_slots & (1u << slot)) ? (int)slot : -1;
}
FRESULT f_open(FIL *f, const char *path, unsigned flags) {
  (void)flags; unsigned b,s; ++operations;
  assert(sscanf(path,"bank%u/%u.0.wav.wfm",&b,&s)==2 && s<16);
  if (file_size[s]<0) return FR_NO_FILE;
  *f=(FIL){.bank=b-1,.slot=s,.size=(size_t)file_size[s],.open=1};
  ++handles; return FR_OK;
}
FRESULT f_read(FIL *f, void *data, UINT count, UINT *got) {
  ++operations; assert(f->open && count <= 512);
  if ((int)f->slot == failed_read) { *got=0; return FR_DISK_ERR; }
  *got=count; memcpy(data,file_data[f->slot],count);return FR_OK;
}
FRESULT f_close(FIL *f) {
  ++operations; assert(f->open && handles); --handles;f->open=0;
  return (int)f->slot == failed_close ? FR_DISK_ERR : FR_OK;
}
void sharp_memory_init(void) {}
void sharp_memory_clear(void) {}
bool sharp_memory_async_init(void) { return true; }
bool sharp_memory_busy(void) { return false; }
void sharp_memory_poll(void) {}
bool sharp_memory_submit_rows(unsigned first,unsigned count,const uint8_t *rows) {
  assert(first+count<=240 && count<=8);
  memcpy(image+first*50,rows,count*50);return true;
}
static void setup(void) {
  screen_waveforms_cancel(); assert(!handles);
  bank=(SampleList){.num_samples=16,.valid_slots=65535,.sample=samples};
  for(unsigned b=0;b<16;++b) banks[b]=&bank;
  for(unsigned s=0;s<16;++s) {
    infos[s]=(SampleInfo){.size=8,.num_channels=1,.slice_num=2,
      .slice_start=starts,.slice_stop=stops,.filename_index=s,
      .metadata_checksum=sw_info_hash(screen_golden_info,sizeof screen_golden_info)};
    samples[s].snd[0]=&infos[s];
    memcpy(file_data[s],screen_golden_header,64);
    memset(file_data[s]+64,255,384); file_size[s]=448;
  }
  failed_read=failed_close=-1; operations=0;
}
static void finish(void) {
  unsigned calls=0;
  while(screen_waveforms_pending()) {
    unsigned before=operations;
    screen_waveforms_step(screen_test_now,screen_test_now+250000);
    assert(operations-before<=1); assert(++calls<100);
  }
  assert(!handles);
}
int main(int argc,char **argv) {
  screen_init();
  setup();
  assert(sw_crc32((const uint8_t *)"123456789",9)==0xcbf43926u);
  assert(sw_valid(file_data[0],448,8,44100,2,infos[0].metadata_checksum));
  for(unsigned n=0;n<448;++n) {
    file_data[0][n]^=1;
    assert(!sw_valid(file_data[0],448,8,44100,2,infos[0].metadata_checksum));
    file_data[0][n]^=1;
  }
  for(unsigned n=0;n<448;++n) assert(!sw_valid(file_data[0],n,8,44100,2,infos[0].metadata_checksum));
  screen_waveforms_begin(0,7,0);finish();assert(screen_status.cache_valid==65535);
  setup(); file_size[0]=-1; file_size[2]=447; file_data[3][100]^=1;failed_read=4;failed_close=5;
  screen_waveforms_begin(0,1,0);finish();
  assert(screen_status.cache_valid==(65535u & ~((1u<<0)|(1u<<2)|(1u<<3)|(1u<<4)|(1u<<5))));
  setup(); bank.valid_slots=1u<<9;screen_waveforms_begin(0,9,0);finish();
  assert(screen_status.cache_valid==(1u<<9));
  setup();screen_waveforms_begin(0,7,0);
  assert(!screen_waveforms_step(0,250000) && handles==1);
  assert(screen_waveforms_step(250000,250000) && !handles && screen_waveforms_pending());
  finish();assert(screen_status.cache_valid==65535);
  load.pending=true;load.cursor=16;
  assert(screen_waveforms_step(250000,250000) && !load.pending);
  screen_waveforms_begin(1,2,0);assert(screen_status.cache_valid==0);
  screen_waveforms_step(0,250000);screen_waveforms_cancel();assert(!handles);
  setup(); screen_waveforms_begin(0,0,0); finish(); screen_init();
  screen_snapshot state={.generation=1,.bank=0,.slot=0,.phase=6,.size=8,.bpm=120,.slice=0,
                         .flags=ZS_VALID|ZS_FORWARD};
  screen_publish(&state); screen_test_now=300000;
  for(unsigned n=0;n<30;++n) screen_service(screen_test_now+n*1000);
  assert(screen_status.frames==1 && available && boundary_count==2);
  assert(active_first==0 && active_last==192 && playhead==288);
  if(argc>1) {
    FILE *out=fopen(argv[1],"wb");assert(out);fprintf(out,"P4\n400 240\n");
    for(unsigned n=0;n<sizeof image;++n) {
      uint8_t value=(uint8_t)~image[n], reversed=0;
      for(unsigned b=0;b<8;++b) reversed|=((value>>b)&1u)<<(7-b);
      fputc(reversed,out);
    }
    fclose(out);
  }
  generation=2; screen_service(600000); assert(!available);
  for(unsigned n=1;n<30;++n) screen_service(600000+n*1000);
  state.generation=2;state.flags=0;screen_publish(&state);
  for(unsigned n=0;n<30;++n) screen_service(1000000+n*1000);
  assert(!available && !boundary_count);
  state.flags=ZS_VALID|ZS_FORWARD;screen_publish(&state);
  for(unsigned n=0;n<30;++n) screen_service(1400000+n*1000);
  assert(available && boundary_count==2);
  puts("PASS: golden codec, all corruption/truncation bytes, sparse slots, IO failures, deadline/resume, cancellation, raster and generation checks");
  free(peaks); peaks=NULL;
}
