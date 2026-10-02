// Verify tempo through interruptions and real changes in independent engines.
#include "engine_fixture.h"
#include <stdio.h>

typedef struct {CoreEngine *engine[3];CoreDisplay display[3];unsigned frame;} Chain;
static Chain chain_create(bool trigger,bool slice) {
    Chain c={0};
    for(unsigned i=0;i<3;++i) {
        c.engine[i]=create(123+i);CoreState s;core_engine_get_state(c.engine[i],&s);
        s.clock_stop=true;s.clock_trigger=trigger;s.clock_slice=slice;
        s.division=slice?3:2;s.stopped=false;
        assert(core_engine_set_state(c.engine[i],&s));
    }
    return c;
}
static void check_bpm(const Chain *c,unsigned bpm) {
    for(unsigned i=0;i<3;++i)if(c->display[i].tempo!=bpm) {
        fprintf(stderr,"tempo: frame=%u module=%u expected=%u actual=%u\n",c->frame,i,bpm,c->display[i].tempo);
        assert(c->display[i].tempo==bpm);
    }
}
static void tick(Chain *c,bool input,unsigned bpm) {
    for(unsigned i=0;i<3;++i) {
        int16_t pcm[2];core_engine_clock(c->engine[i],input);
        core_engine_process(c->engine[i],pcm);core_engine_display(c->engine[i],&c->display[i]);
        input=c->display[i].clock;
    }
    ++c->frame;if(bpm)check_bpm(c,bpm);
}
static unsigned period_for(unsigned bpm) { return (unsigned)lround(CORE_RATE*30.0/bpm); }
static void drive(Chain *c,unsigned bpm,unsigned pulses,bool settled) {
    unsigned period=period_for(bpm);
    for(unsigned f=0;f<period*pulses;++f)tick(c,f%period<200,settled?bpm:0);
    check_bpm(c,bpm);
}
static void pause_clock(Chain *c,unsigned frames,unsigned bpm,bool stopped) {
    for(unsigned f=0;f<frames;++f)tick(c,false,bpm);
    if(stopped)for(unsigned i=0;i<3;++i)assert(!c->display[i].clock);
}
static void check_restart(Chain *c,unsigned bpm) {
    // Check before rendering advances: the first returning edge resets every
    // module's slice and clock, without waiting for tempo confirmation.
    bool input=true;
    for(unsigned i=0;i<3;++i) {
        core_engine_clock(c->engine[i],input);core_engine_display(c->engine[i],&c->display[i]);
        assert(c->display[i].slice==0&&c->display[i].clock);input=c->display[i].clock;
    }
    check_bpm(c,bpm);
}
static void destroy(Chain *c) { for(unsigned i=0;i<3;++i)core_engine_destroy(c->engine[i]); }
static void check_same_tempo(unsigned bpm,bool trigger,bool slice) {
    Chain c=chain_create(trigger,slice);drive(&c,bpm,80,false);
    unsigned period=period_for(bpm);
    const unsigned pauses[]={period/2,period-1,period+1,period*2,period*12};
    for(unsigned cycle=0;cycle<2;++cycle)for(unsigned p=0;p<sizeof pauses/sizeof pauses[0];++p) {
        bool stopped=p==4;
        pause_clock(&c,pauses[p],bpm,stopped);
        if(stopped)check_restart(&c,bpm);
        drive(&c,bpm,16,true);
    }
    destroy(&c);
}
static void check_changed_tempo(bool stopped) {
    Chain c=chain_create(false,false);drive(&c,120,80,false);
    const unsigned tempos[]={90,180,60,120,240,120};
    unsigned previous=120;
    for(unsigned i=0;i<sizeof tempos/sizeof tempos[0];++i) {
        if(stopped){pause_clock(&c,period_for(previous)*12,previous,true);check_restart(&c,previous);}
        drive(&c,tempos[i],100,false);
        drive(&c,tempos[i],8,true);previous=tempos[i];
    }
    destroy(&c);
}
static void check_independent_candidates(void) {
    CoreEngine *a=create(111),*b=create(222);CoreState sa,sb;
    for(unsigned f=0;f<CORE_RATE*16;++f) {
        int16_t pcm[2];core_engine_clock(a,f%11025<200);core_engine_process(a,pcm);
        core_engine_clock(b,f%7350<200);core_engine_process(b,pcm);
    }
    // A sees a single longer interval; B continues its unrelated 180 BPM clock.
    for(unsigned f=0;f<CORE_RATE*2;++f) {
        int16_t pcm[2];core_engine_clock(a,f>=4410&&(f-4410)%11025<200);core_engine_process(a,pcm);
        core_engine_clock(b,f%7350<200);core_engine_process(b,pcm);
        core_engine_get_state(a,&sa);core_engine_get_state(b,&sb);
        assert(sa.tempo==120&&sb.tempo==180);
    }
    core_engine_destroy(a);core_engine_destroy(b);
}
int main(void) {
    const unsigned tempos[]={60,90,120,180,240};
    for(unsigned i=0;i<sizeof tempos/sizeof tempos[0];++i)check_same_tempo(tempos[i],false,false);
    for(unsigned trigger=0;trigger<2;++trigger)for(unsigned slice=0;slice<2;++slice)
        if(trigger||slice)check_same_tempo(120,trigger,slice);
    check_changed_tempo(false);check_changed_tempo(true);check_independent_candidates();
    puts("tempo: 60/90/120/180/240 BPM, repeated short/long stops, slice-zero restart, real rate changes and independent chains passed");
}
