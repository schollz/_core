// The handoff must protect file-like mutable state through rapid reacquisition,
// rejected optional jobs, timeouts, and callbacks already in progress.
#include "audio_media_owner.h"
#include <assert.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdio.h>
#include <time.h>
static _Atomic bool stop, quiet, audio_inside, control_inside;
static _Atomic unsigned callbacks;
static unsigned payload[128];
static void *audio(void *unused) {
    (void)unused;
    while(!atomic_load(&stop)) {
        if(audio_media_begin()) {
            atomic_store(&audio_inside,true);assert(!atomic_load(&control_inside));
            unsigned value=payload[0];
            for(unsigned i=1;i<128;++i)assert(payload[i]==value);
            if((atomic_load(&callbacks)&15)==0)sched_yield();
            assert(!atomic_load(&control_inside));atomic_store(&audio_inside,false);
        }
        audio_media_end(atomic_load(&quiet));atomic_fetch_add(&callbacks,1);
        sched_yield();
    }
    return NULL;
}
int main(void) {
    pthread_t thread;assert(!pthread_create(&thread,NULL,audio,NULL));
    while(atomic_load(&callbacks)<100)sched_yield();
    assert(audio_media_control_owned()&&!audio_media_timer_allowed());
    assert(audio_media_acquire());audio_media_release();
    assert(!atomic_load(&audio_inside));audio_media_boot_complete();
    for(unsigned pass=1;pass<=20000;++pass) {
        assert(audio_media_acquire());assert(!atomic_load(&audio_inside));
        atomic_store(&control_inside,true);
        for(unsigned i=0;i<128;++i)payload[i]=pass;
        if((pass&15)==0)sched_yield();
        atomic_store(&control_inside,false);audio_media_release();
        if(pass%100==0) {
            atomic_store(&quiet,false);assert(!audio_media_try_quiet());
            atomic_store(&quiet,true);assert(audio_media_try_quiet());
            assert(!atomic_load(&audio_inside));audio_media_release();
        }
    }
    atomic_store(&stop,true);assert(!pthread_join(thread,NULL));
    assert(!audio_media_acquire()); // a missing audio owner cannot grant access
    assert(audio_media_stats.timeouts==1&&audio_media_stats.quiet_rejected==200);
    printf("ownership: 20200 acknowledged handoffs, 200 rejected quiet jobs, timeout, publication and nesting pass\n");
}
