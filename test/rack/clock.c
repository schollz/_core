// Clock-loss behavior through the shared firmware engine, including a CV chain.
#include "engine_fixture.h"
#include <stdio.h>
#include <string.h>

// Exercise the production predicate's exact boundaries and MIDI exceptions.
static bool clock_in_do, clock_start_stop_sync, use_onewiremidi, usb_midi_present;
static uint32_t clock_in_last_time, clock_in_diff_2x;
#include "../../lib/clock_stop.h"

static void check_predicate(void) {
    clock_in_do=clock_start_stop_sync=true;
    clock_in_last_time=1000;clock_in_diff_2x=500000;
    assert(!clock_input_should_stop(500999));
    assert(!clock_input_should_stop(501000));
    assert(clock_input_should_stop(501001));
    clock_start_stop_sync=false;assert(!clock_input_should_stop(501001));
    clock_start_stop_sync=true;clock_in_do=false;assert(!clock_input_should_stop(501001));
    clock_in_do=true;use_onewiremidi=true;assert(!clock_input_should_stop(501001));
    use_onewiremidi=false;usb_midi_present=true;assert(!clock_input_should_stop(501001));
    usb_midi_present=false;
    clock_in_last_time=UINT32_MAX-1000;
    assert(!clock_input_should_stop(clock_in_last_time+clock_in_diff_2x));
    assert(clock_input_should_stop(clock_in_last_time+clock_in_diff_2x+1));
    puts("clock: timeout boundaries, timestamp rollover, inactive input and MIDI exceptions passed");
}

typedef struct {unsigned edges, audible;bool high;} Trace;
static CoreDisplay step(CoreEngine *e,bool input,Trace *trace) {
    int16_t pcm[2];CoreDisplay d;
    core_engine_clock(e,input);core_engine_process(e,pcm);core_engine_display(e,&d);
    trace->edges+=d.clock&&!trace->high;trace->high=d.clock;
    trace->audible+=pcm[0]!=0||pcm[1]!=0;
    return d;
}
static Trace run(CoreEngine *e,unsigned frames,bool clocked,bool silent) {
    CoreDisplay d;core_engine_display(e,&d);Trace trace={.high=d.clock};
    for(unsigned f=0;f<frames;++f) {
        d=step(e,clocked&&f%(CORE_RATE/4)<441,&trace);
        if(silent)assert(!d.clock);
    }
    if(silent)assert(!trace.audible&&!trace.edges);
    return trace;
}
static CoreEngine *configured(bool stop,bool trigger,bool slice,unsigned division) {
    CoreEngine *e=create(123);CoreState s;core_engine_get_state(e,&s);
    s.clock_stop=stop;s.clock_trigger=trigger;s.clock_slice=slice;s.division=division;
    s.stopped=false;s.muted=false;s.tempo=120;
    assert(core_engine_set_state(e,&s));return e;
}
static void check_stop_restart(bool trigger,bool slice,unsigned division) {
    CoreEngine *e=configured(true,trigger,slice,division);
    // Acquire the input interval, then align all output divisions on a restart.
    run(e,CORE_RATE,true,false);run(e,CORE_RATE,false,false);
    Trace active=run(e,CORE_RATE,true,false);
    assert((active.edges||active.high)&&active.audible);
    run(e,CORE_RATE/5,false,false);
    CoreDisplay d;core_engine_display(e,&d);
    // The slow tempo square wave is still high just before clock-loss timeout.
    if(!trigger&&!slice&&division==6)assert(d.clock);
    // A 100 ms trigger has already gone low before this same timeout.
    if(trigger&&division==6)assert(!d.clock);
    run(e,CORE_RATE/5,false,false);
    run(e,CORE_RATE*2,false,true);
    core_engine_clock(e,true);core_engine_display(e,&d);
    assert(d.clock&&d.slice==0); // First returning edge resets phase and clock.
    Trace restarted=run(e,CORE_RATE,true,false);assert(restarted.audible);
    core_engine_destroy(e);
}
static void check_free_running(void) {
    for(unsigned trigger=0;trigger<2;++trigger)for(unsigned slice=0;slice<2;++slice) {
        CoreEngine *e=configured(true,trigger,slice,2);
        Trace internal=run(e,CORE_RATE*2,false,false);
        assert(internal.edges&&internal.audible); // No external clock acquired.
        run(e,CORE_RATE,true,false);run(e,CORE_RATE,false,false);
        run(e,CORE_RATE,false,true);
        CoreState s;core_engine_get_state(e,&s);s.clock_stop=false;
        assert(core_engine_update_settings(e,&s));
        Trace resumed=run(e,CORE_RATE*2,false,false);
        assert(resumed.edges&&resumed.audible);
        run(e,CORE_RATE,true,false);run(e,CORE_RATE,false,false);
        Trace continuing=run(e,CORE_RATE,false,false);
        assert(continuing.edges&&continuing.audible);
        // Button mute is independent of clock-loss synchronization.
        core_engine_get_state(e,&s);s.clock_stop=true;s.muted=true;
        assert(core_engine_set_state(e,&s));
        Trace muted=run(e,CORE_RATE,true,false);assert(muted.edges);
        muted=run(e,CORE_RATE,true,false);assert(muted.edges&&!muted.audible);
        core_engine_destroy(e);
    }
    puts("clock: internal clock, disabled stop sync, live setting changes and ordinary mute passed");
}
static void check_missing_media(void) {
    CoreEngine *e=configured(true,false,false,6);
    run(e,CORE_RATE,true,false);run(e,CORE_RATE,false,false);
    run(e,CORE_RATE,true,false);
    CoreDisplay d;core_engine_display(e,&d);assert(d.clock);
    CoreBank empty[16]={0};core_engine_set_catalogue(e,empty,read_pcm,0);
    run(e,CORE_RATE,false,false);
    run(e,CORE_RATE,false,true); // Timeout still releases GPIO before media return.
    core_engine_destroy(e);
}
static void check_chain(bool trigger,bool slice) {
    CoreEngine *chain[3];Trace traces[3]={0};CoreDisplay d[3];
    // Slice-following uses /1 here so output remains the input's 2 PPQN.
    for(unsigned i=0;i<3;++i)chain[i]=configured(true,trigger,slice,slice?3:2);
    for(unsigned f=0;f<CORE_RATE*6;++f) {
        bool input=f<CORE_RATE*3&&f%(CORE_RATE/4)<441;
        if(f==CORE_RATE*5)memset(traces,0,sizeof traces);
        for(unsigned i=0;i<3;++i) {
            d[i]=step(chain[i],input,&traces[i]);input=d[i].clock;
            if(f>=CORE_RATE*5)assert(!d[i].clock);
        }
        if(f==CORE_RATE*3-1)for(unsigned i=0;i<3;++i)assert(traces[i].edges&&traces[i].audible);
    }
    for(unsigned i=0;i<3;++i)assert(!traces[i].edges&&!traces[i].audible);
    bool input=true;
    for(unsigned i=0;i<3;++i) {
        core_engine_clock(chain[i],input);core_engine_display(chain[i],&d[i]);
        assert(d[i].clock&&d[i].slice==0);input=d[i].clock;
    }
    for(unsigned f=0;f<CORE_RATE;++f) {
        input=f%(CORE_RATE/4)<441;
        for(unsigned i=0;i<3;++i){d[i]=step(chain[i],input,&traces[i]);input=d[i].clock;}
    }
    for(unsigned i=0;i<3;++i){assert(traces[i].edges&&traces[i].audible);core_engine_destroy(chain[i]);}
}
int main(void) {
    check_predicate();
    for(unsigned trigger=0;trigger<2;++trigger)for(unsigned slice=0;slice<2;++slice) {
        check_stop_restart(trigger,slice,2);check_stop_restart(trigger,slice,6);
        check_chain(trigger,slice);
    }
    check_free_running();check_missing_media();
    puts("clock: square/trigger, tempo/slices, slow divisions, missing media and three-module stop/restart at slice zero passed");
}
