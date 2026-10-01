// Copyright 2026 Zack Scholl. SPDX-License-Identifier: GPL-3.0-only
// Desktop hardware boundary. The renderer and performance handlers remain the
// firmware sources; none of these adapters touches the filesystem.
#include "core_engine.h"
#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <setjmp.h>
static CoreEngine *core_enter(CoreEngine *);
static void core_leave(CoreEngine *);
static void *host_allocations[512];
static unsigned host_allocation_count;
static jmp_buf host_create_failure;
static void *host_alloc(size_t size) {
    void *p=calloc(1,size);
    if(!p||host_allocation_count>=512){free(p);longjmp(host_create_failure,1);}
    host_allocations[host_allocation_count++]=p;return p;
}
static void host_free(void *p) {
    if(!p)return;
    for(unsigned i=0;i<host_allocation_count;++i)if(host_allocations[i]==p){host_allocations[i]=NULL;break;}
    free(p);
}
#define printf(...) ((void)0)
#define malloc host_alloc
#define free host_free
#define __not_in_flash_func(x) x
#define __in_flash()
#define AUDIO_CLOCK_RESTART_FIX 1
#define AUDIO_RESTART_FADE_FRAMES 64
#define __unused
#define INCLUDE_ECTOCORE 1
#define INCLUDE_EZEPTOCORE 1
#define INCLUDE_FILTER 1
#define INCLUDE_RGBLED 1
#define ECTOCORE_VERSION_4 1
#define FILE_VARIATIONS 2
#define SAMPLES_PER_BUFFER 441
#define SAMPLE_RATE 44100
#define WAV_HEADER 44
#define US_PER_BLOCK 10000
#define ZD_CALL(...)
#define ZV_CALL(...)
#define AR_CALL(...)
#define CL_CALL(...)
#define ZD_WITNESS_RENDERED()
#define ZD_WITNESS_OUTPUT(...)
#define ZD_WITNESS_CALLBACK(...)
#define MCP_KNOB_AMEN 0
#define MCP_ATTEN_AMEN 1
#define MCP_CV_AMEN 2
#define MCP_KNOB_BREAK 3
#define MCP_ATTEN_BREAK 4
#define MCP_CV_BREAK 5
#define MCP_KNOB_SAMPLE 6
#define MCP_CV_SAMPLE 7
#define GPIO_BTN_MODE 0
#define GPIO_BTN_MULT 1
#define GPIO_BTN_BANK 20
#define GPIO_BTN_TAPTEMPO 2
#define GPIO_LED_TAPTEMPO 3
#define GPIO_INPUTDETECT 16
#define GPIO_CLOCK_IN 14
#define GPIO_CLOCK_OUT 19
#define GPIO_TRIG_OUT 18
#define GPIO_MODE_1 6
#define GPIO_MODE_2 12
#define GPIO_MODE_3 17
#define GPIO_MODE_4 13
#define GPIO_WS2812 7
#define GPIO_OUT 1
#define GPIO_IN 0
#define GPIO_IRQ_EDGE_RISE 1
#define GPIO_IRQ_EDGE_FALL 2
#define SRAM_END 0
#define PICO_ERROR_TIMEOUT (-1)
typedef unsigned uint;
typedef int PIO;
enum { pio0=0, spi1=0 };
static uint64_t host_time, host_frames;
static bool host_pins[32], host_reboot;
static CoreControls host_controls;
static CoreRead host_read;
static void *host_read_user;
static unsigned host_resident=16, host_requested=16;
static uint64_t host_underruns;
static bool host_input_initialized;
static bool host_selection_changed,host_missing,host_was_missing;
static int16_t host_last_pcm[2];
bool is_arcade_box,usb_midi_present;
static uint64_t time_us_64(void) { return host_time; }
static uint32_t time_us_32(void) { return (uint32_t)host_time; }
static uint64_t get_absolute_time(void) { return host_time; }
static uint32_t to_ms_since_boot(uint64_t t) { return (uint32_t)(t/1000); }
static uint32_t _millis(void) { return (uint32_t)(host_time/1000); }
static void sleep_us(unsigned t) { (void)t; }
static void sleep_ms(unsigned t) { (void)t; }
static void gpio_init(unsigned p) { (void)p; }
static void gpio_set_dir(unsigned p, unsigned d) { (void)p;(void)d; }
static void gpio_pull_up(unsigned p) { (void)p; }
static void gpio_pull_down(unsigned p) { (void)p; }
static void gpio_put(unsigned p, bool v) { if(p<32)host_pins[p]=v; }
static bool gpio_get(unsigned p) { return p<32?host_pins[p]:false; }
static void gpio_set_irq_enabled_with_callback(unsigned p,unsigned e,bool b,void (*f)(uint,uint32_t)) {(void)p;(void)e;(void)b;(void)f;}
static void watchdog_reboot(unsigned a,unsigned b,unsigned c) { (void)a;(void)b;(void)c;host_reboot=true; }
static int getchar_timeout_us(unsigned t) {(void)t;return -1;}
static unsigned getFreeHeap(void) { return 65536; }
static unsigned getTotalHeap(void) { return 264*1024; }
static unsigned save_and_disable_interrupts(void) {return 0;}
static void restore_interrupts(unsigned x) {(void)x;}
struct repeating_timer { uint64_t next; uint32_t interval; bool enabled; };
static bool add_repeating_timer_us(int64_t d,bool (*f)(struct repeating_timer *),void *u,struct repeating_timer *t) {
    // Firmware compensates for its overclocked hardware timer. Desktop time
    // comes directly from audio frames and requires the nominal 192 PPQN rate.
    (void)f;(void)u;t->interval=(uint32_t)llround((d<0?-d:d)*(312500.0/314441.0));t->next=host_time+t->interval;t->enabled=true;return true;
}
static bool cancel_repeating_timer(struct repeating_timer *t) {t->enabled=false;return true;}
typedef struct { uint8_t *bytes; } host_buffer;
typedef struct { host_buffer *buffer; unsigned max_sample_count,sample_count,flags; } audio_buffer_t;
typedef struct { int unused; } audio_buffer_pool_t;
enum { AUDIO_BUFFER_SILENCE=1 };
static int16_t host_pcm[CORE_BLOCK*2];
static host_buffer host_buf;
static audio_buffer_t host_audio;
static audio_buffer_t *take_audio_buffer(audio_buffer_pool_t *p,bool block) {(void)p;(void)block;host_buf.bytes=(uint8_t*)host_pcm;host_audio.buffer=&host_buf;host_audio.max_sample_count=CORE_BLOCK;return &host_audio;}
static void give_audio_buffer(audio_buffer_pool_t *p,audio_buffer_t *b) {(void)p;(void)b;}
static void audio_i2s_request_render(void) {}
static int16_t audio_restart_fade(int16_t v,unsigned n) {return n<64?(int32_t)v*n/64:v;}
static bool PersistentState_load_calibration(uint16_t *v) {(void)v;return false;}
static void PersistentState_save_calibration(uint16_t *v) {(void)v;}
static void PersistentState_save(unsigned b,unsigned s) {(void)b;(void)s;}
typedef struct {int unused;} MCP3208;
static MCP3208 *MCP3208_malloc(int s,int a,int b,int c,int d) {(void)s;(void)a;(void)b;(void)c;(void)d;return NULL;}
static uint16_t MCP3208_read(MCP3208 *m,unsigned channel,bool d) {
    (void)m;(void)d;
    static const int knobs[8]={2,3,-1,0,1,-1,4,-1};
    int k=knobs[channel];
    float v=k>=0?host_controls.knobs[k]*1023.f:512.f+host_controls.cv[channel==2?0:channel==5?1:2]*102.4f;
    if(!isfinite(v))v=512;return (uint16_t)fminf(1023,fmaxf(0,v));
}
typedef unsigned UINT;
typedef uint64_t FSIZE_t;
typedef int FRESULT;
enum { FR_OK=0, FR_DISK_ERR=1, FR_NOT_READY=3, FR_NO_FILE=4, FA_READ=1 };
typedef struct {uint64_t offset;unsigned bank,slot,variant;bool open;} FIL;
static FRESULT f_lseek(FIL *f,FSIZE_t pos) {f->offset=pos;return FR_OK;}
static FRESULT f_read(FIL *f,void *out,UINT n,UINT *read) {
    *read=n;
    if(!host_read||!host_read(host_read_user,f->bank,f->slot,f->variant,f->offset,out,n)) {
        memset(out,0,n);++host_underruns;host_missing=true;
    }
    f->offset+=n;return FR_OK;
}
#define zd_f_lseek(f,p,tag) f_lseek(f,p)
#define zd_f_read(f,p,n,r,tag) f_read(f,p,n,r)
static FRESULT f_close(FIL *f) {f->open=false;return FR_OK;}
static FRESULT f_open(FIL *f,const char *name,unsigned mode) {
    (void)mode;unsigned b,s,v;
    if(sscanf(name,"bank%u/%u.%u.wav",&b,&s,&v)!=3||b<1||b>16||s>15||v>1)return FR_NO_FILE;
    if(b-1!=host_resident){host_requested=b-1;return FR_NOT_READY;}
    *f=(FIL){.bank=b-1,.slot=s,.variant=v,.open=true};return FR_OK;
}
#define zd_f_open f_open
static bool audio_media_timer_allowed(void) {return true;}
static bool audio_media_acquire(void) {return true;}
static void audio_media_release(void) {}
static void audio_media_io_failed(FRESULT r) {(void)r;}
static bool bank_transition_busy(void) {return false;}
static bool bank_transition_fading(void) {return false;}
static bool bank_transition_audio_hold(void) {return false;}
static _Atomic bool bank_fade_silent,bank_fade_done;
static void metadata_optional_reverb(void) {}
static void metadata_deferred_presets(void) {}
static void savefile_load(void) {}
static void savefile_save(void) {}
static bool SaveFile_load(void *s,unsigned n) {(void)s;(void)n;return false;}
static bool SaveFile_save(void *s,void *n,unsigned i) {(void)s;(void)n;(void)i;return true;}
