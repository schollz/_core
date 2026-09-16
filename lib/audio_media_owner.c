// Copyright 2026 Zack Scholl, GPLv3.0
#include "audio_media_owner.h"
#include <assert.h>
#include <stdatomic.h>
#ifdef AUDIO_MEDIA_HOST_TEST
#include <time.h>
static uint32_t owner_now(void) {
    struct timespec now;clock_gettime(CLOCK_MONOTONIC,&now);
    return (uint64_t)now.tv_sec*1000000+now.tv_nsec/1000;
}
static void owner_wait(void) {
    const struct timespec delay={0,50000};nanosleep(&delay,NULL);
}
#define OWNER_RAM(name) name
#else
#include "pico/stdlib.h"
#define owner_now time_us_32
static void owner_wait(void) { sleep_us(50); }
#define OWNER_RAM(name) __not_in_flash_func(name)
#endif

// Each word has one writer. Use aligned atomic loads/stores, never an atomic
// read-modify-write or a spinlock held over I/O on ARMv6-M.
static _Atomic uint32_t requested=1, acknowledged, released, rejected;
static _Atomic bool boot_complete, quiet_request;
static volatile uint32_t depth=1; // core 0 and its timer IRQ only
audio_media_owner_stats audio_media_stats;

bool audio_media_control_owned(void) { return depth!=0; }
bool audio_media_timer_allowed(void) {
    return !depth&&atomic_load_explicit(&boot_complete,memory_order_acquire);
}
static bool acquire(bool quiet) {
    if(depth) {++depth;return true;}
    uint32_t request=atomic_load_explicit(&requested,memory_order_relaxed)+1;
    atomic_store_explicit(&quiet_request,quiet,memory_order_relaxed);
    atomic_store_explicit(&requested,request,memory_order_release);
    uint32_t start=owner_now();
    while(atomic_load_explicit(&acknowledged,memory_order_acquire)!=request) {
        if(quiet&&atomic_load_explicit(&rejected,memory_order_acquire)==request) {
            ++audio_media_stats.quiet_rejected;
            atomic_store_explicit(&released,request,memory_order_release);return false;
        }
        if((uint32_t)(owner_now()-start)>=100000) {
            ++audio_media_stats.timeouts;
            atomic_store_explicit(&released,request,memory_order_release);return false;
        }
        owner_wait();
    }
    uint32_t waited=owner_now()-start;
    if(waited>audio_media_stats.wait_max_us)audio_media_stats.wait_max_us=waited;
    depth=1;++audio_media_stats.acquisitions;return true;
}
bool audio_media_acquire(void) { return acquire(false); }
bool audio_media_try_quiet(void) { return acquire(true); }
void audio_media_release(void) {
    assert(depth);
    if(--depth)return;
    atomic_store_explicit(&released,atomic_load_explicit(&requested,memory_order_relaxed),memory_order_release);
}
void audio_media_boot_complete(void) {
    assert(depth==1);
    atomic_store_explicit(&boot_complete,true,memory_order_release);
    audio_media_release();
}
bool OWNER_RAM(audio_media_begin)(void) {
    uint32_t request=atomic_load_explicit(&requested,memory_order_acquire);
    bool boot=atomic_load_explicit(&boot_complete,memory_order_acquire);
    bool held=request!=atomic_load_explicit(&released,memory_order_acquire)&&
        request==atomic_load_explicit(&acknowledged,memory_order_relaxed);
    if(!boot||held) {++audio_media_stats.withheld_callbacks;return false;}
    return true;
}
void OWNER_RAM(audio_media_end)(bool quiescent) {
    uint32_t request=atomic_load_explicit(&requested,memory_order_acquire);
    if(request==atomic_load_explicit(&released,memory_order_acquire))return;
    // A new request cannot reuse the previous generation's acknowledgement.
    if(atomic_load_explicit(&quiet_request,memory_order_relaxed)&&!quiescent)
        atomic_store_explicit(&rejected,request,memory_order_release);
    else atomic_store_explicit(&acknowledged,request,memory_order_release);
}
