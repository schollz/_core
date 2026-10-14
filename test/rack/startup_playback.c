// Startup policy and transport through the production firmware engine.
#include "engine_fixture.h"
#include <stdio.h>

static CoreDisplay run(CoreEngine *e, unsigned frames, bool silent) {
    CoreDisplay d = {0};
    for(unsigned i=0;i<frames;++i) {
        int16_t pcm[2];core_engine_process(e,pcm);core_engine_display(e,&d);
        if(silent)assert(!pcm[0]&&!pcm[1]&&!d.clock&&!d.trigger);
    }
    return d;
}
static void audible(CoreEngine *e) {
    unsigned sound=0;
    for(unsigned i=0;i<CORE_RATE/2;++i) {
        int16_t pcm[2];core_engine_process(e,pcm);sound+=pcm[0]!=0||pcm[1]!=0;
    }
    assert(sound);
}
static CoreEngine *waiting(bool stop,bool trigger,bool slice) {
    CoreEngine *e=create(123);CoreState s;core_engine_get_state(e,&s);
    assert(!s.startup_wait);
    s.startup_wait=true;s.clock_stop=stop;s.clock_trigger=trigger;s.clock_slice=slice;
    s.start_tempo=130;assert(core_engine_update_settings(e,&s));
    core_engine_apply_start_tempo(e);core_engine_apply_startup_playback(e);
    core_engine_get_state(e,&s);assert(s.startup_wait&&s.stopped&&s.muted&&s.tempo==130);
    return e;
}
static void check_clock(bool stop,bool trigger,bool slice) {
    CoreEngine *e=waiting(stop,trigger,slice);
    run(e,CORE_RATE*2,true);
    // An unchanged low input, ordinary settings edits, and media attachment
    // must not release the startup hold.
    core_engine_clock(e,false);
    CoreState s;core_engine_get_state(e,&s);s.brightness=75;
    assert(core_engine_update_settings(e,&s));core_engine_set_resident_bank(e,0);
    run(e,CORE_RATE/10,true);
    core_engine_clock(e,true);
    CoreDisplay d;core_engine_display(e,&d);assert(!d.stopped&&!d.muted&&d.slice==0&&d.clock);
    unsigned sound=0;
    for(unsigned i=0;i<CORE_RATE;++i) {
        core_engine_clock(e,i%(CORE_RATE/4)<441);
        int16_t pcm[2];core_engine_process(e,pcm);sound+=pcm[0]!=0||pcm[1]!=0;
    }
    assert(sound);
    // Once playing, a settings edit must not reapply the startup policy.
    core_engine_get_state(e,&s);s.startup_wait=false;assert(core_engine_update_settings(e,&s));
    s.startup_wait=true;assert(core_engine_update_settings(e,&s));
    core_engine_get_state(e,&s);assert(!s.stopped&&!s.muted);
    core_engine_clock(e,false);run(e,CORE_RATE,false);
    if(stop) {
        run(e,CORE_RATE,true);
        core_engine_clock(e,true);core_engine_display(e,&d);assert(!d.stopped&&!d.muted&&d.slice==0);
        audible(e);
    } else audible(e);
    core_engine_destroy(e);
}
static void check_button(void) {
    CoreEngine *e=waiting(true,false,false);run(e,CORE_RATE/10,true);
    CoreControls c={.knobs={0,0,0,.5f,0},.buttons={false,false,false,true}};
    core_engine_controls(e,&c);run(e,CORE_RATE/10,false);
    c.buttons[3]=false;core_engine_controls(e,&c);
    CoreState s;core_engine_get_state(e,&s);assert(!s.stopped&&!s.muted);
    audible(e);core_engine_destroy(e);
}
static void check_immediate_clock(void) {
    CoreEngine *e=waiting(true,false,false);
    // Rack can reboot while the next rising edge is already being processed,
    // before the new engine has advanced its first audio frame.
    core_engine_clock(e,true);
    CoreDisplay d;core_engine_display(e,&d);assert(!d.stopped&&!d.muted&&d.slice==0&&d.clock);
    audible(e);core_engine_destroy(e);
}
static void check_default(void) {
    CoreEngine *e=create(456);core_engine_apply_startup_playback(e);audible(e);
    CoreState s;core_engine_get_state(e,&s);assert(!s.startup_wait&&!s.stopped);
    // The default leaves saved stopped/muted state intact.
    s.stopped=s.muted=true;assert(core_engine_set_state(e,&s));
    core_engine_apply_startup_playback(e);run(e,CORE_RATE/10,false);
    core_engine_get_state(e,&s);assert(s.stopped&&s.muted);core_engine_destroy(e);
}
static void check_chain(void) {
    CoreEngine *chain[3];unsigned sound[3]={0};
    for(unsigned i=0;i<3;++i){chain[i]=waiting(true,false,false);run(chain[i],CORE_RATE,true);}
    for(unsigned f=0;f<CORE_RATE*2;++f) {
        bool input=f%(CORE_RATE/4)<441;
        for(unsigned i=0;i<3;++i) {
            core_engine_clock(chain[i],input);
            int16_t pcm[2];CoreDisplay d;core_engine_process(chain[i],pcm);core_engine_display(chain[i],&d);
            if(f==0)assert(!d.stopped&&!d.muted&&d.slice==0&&d.clock);
            sound[i]+=pcm[0]!=0||pcm[1]!=0;input=d.clock;
        }
    }
    for(unsigned i=0;i<3;++i){assert(sound[i]);core_engine_destroy(chain[i]);}
}
int main(void) {
    check_default();check_button();check_immediate_clock();
    for(unsigned stop=0;stop<2;++stop)for(unsigned trigger=0;trigger<2;++trigger)for(unsigned slice=0;slice<2;++slice)
        check_clock(stop,trigger,slice);
    check_chain();
    puts("Startup playback: silent wait, button/first-clock start, output modes, clock loss, runtime settings and three-module chain passed");
}
