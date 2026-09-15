#include "audio_profile.h"
#if AUDIO_DETAILED_TIMING
#include "seek_diagnostics.h"
#include <stdatomic.h>
#include <string.h>
volatile audio_profile_record audio_profile_records[4];
static audio_profile_record current,previous;
static _Atomic uint32_t event_sequence,event_time;
static uint32_t seen_event,callback_sequence,baseline[6];
static bool active;
static const unsigned metric[6]={ZD_OPEN,ZD_CLOSE,ZD_SEEK,ZD_READ,ZD_STRETCH_SEEK,ZD_STRETCH_READ};
static void publish(unsigned slot,const audio_profile_record *record) {
    volatile audio_profile_record *out=&audio_profile_records[slot];
    uint32_t seq=out->sequence;
    out->sequence=seq+1;atomic_thread_fence(memory_order_release);
    const uint32_t *src=(const uint32_t *)record;
    volatile uint32_t *dst=(volatile uint32_t *)out;
    for(unsigned i=1;i<sizeof(*out)/4;++i)dst[i]=src[i];
    atomic_thread_fence(memory_order_release);out->sequence=seq+2;
}
static void capture_event(audio_profile_record *record,unsigned relation) {
    uint32_t event=atomic_load_explicit(&event_sequence,memory_order_acquire);
    if(event==seen_event)return;
    record->event=event;record->event_us=atomic_load_explicit(&event_time,memory_order_relaxed);
    record->relation=relation==1&&(uint32_t)(record->event_us-record->start_us)>record->total_us?2:relation;
    publish(1+(event&1),record);seen_event=event;
}
void audio_profile_begin(bool enabled,uint32_t source,uint32_t effects,
                         uint32_t pitch,uint32_t stretch) {
    if(previous.callback)capture_event(&previous,2); // between completed callbacks
    active=enabled;
    memset(&current,0,sizeof current);
    if(!active)return;
    current.callback=++callback_sequence;current.start_us=time_us_32();
    current.source_before=source;current.effects_before=effects;
    current.pitch=pitch;current.stretch=stretch;
    for(unsigned i=0;i<6;++i)baseline[i]=zd_audio.metrics[metric[i]].total_lo;
}
void audio_profile_add(unsigned stage,uint32_t started) {
    if(active&&stage<AP_COUNT)current.us[stage]+=time_us_32()-started;
}
void audio_profile_end(uint32_t source,uint32_t effects) {
    if(!active)return;
    current.total_us=time_us_32()-current.start_us;
    current.source_after=source;current.effects_after=effects;
    for(unsigned i=0;i<6;++i)current.us[i]=zd_audio.metrics[metric[i]].total_lo-baseline[i];
    capture_event(&current,1); // callback spans a DMA starvation notification
    if(current.total_us>audio_profile_records[0].total_us)publish(0,&current);
    if(!(current.callback&255))publish(3,&current);
    previous=current;active=false;
}
void audio_profile_starved(uint32_t now) {
    atomic_store_explicit(&event_time,now,memory_order_relaxed);
    atomic_store_explicit(&event_sequence,
        atomic_load_explicit(&event_sequence,memory_order_relaxed)+1,memory_order_release);
}
#endif
