// Independent instances must produce the same PCM regardless of interleaving.
#include "core_engine.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include "engine_fixture.h"
typedef struct {CoreEngine *engine;uint64_t hash;} ThreadTrace;
static void *thread_trace(void *arg){
    ThreadTrace *trace=arg;trace->hash=1469598103934665603ULL;
    for(unsigned i=0;i<CORE_RATE;++i){int16_t pcm[2];core_engine_process(trace->engine,pcm);for(unsigned c=0;c<2;++c)trace->hash=(trace->hash^(uint16_t)pcm[c])*1099511628211ULL;}
    return NULL;
}
static void check_led_brightness(void) {
    CoreEngine *a=create(901),*b=create(901);
    CoreControls controls={.knobs={.75f,.5f,0,.5f,0}};
    core_engine_controls(a,&controls);core_engine_controls(b,&controls);
    for(unsigned frame=0;frame<CORE_RATE/2;++frame){int16_t pcm[2];core_engine_process(a,pcm);core_engine_process(b,pcm);}
    CoreDisplay original,display,unrelated;core_engine_display(a,&original);core_engine_display(b,&unrelated);
    assert(!memcmp(original.rgb,unrelated.rgb,sizeof original.rgb));
    unsigned energy=0;for(unsigned i=0;i<18;++i)for(unsigned c=0;c<3;++c)energy+=original.rgb[i][c];
    assert(energy>0);
    CoreState state;core_engine_get_state(a,&state);assert(state.brightness==50);
    unsigned previous=0;
    for(unsigned brightness=0;brightness<=100;brightness+=25){
        state.brightness=brightness;assert(core_engine_update_settings(a,&state));
        // No audio/control tick or knob movement: even a held pattern must
        // immediately follow the menu setting and survive an off/on cycle.
        core_engine_display(a,&display);energy=0;
        for(unsigned i=0;i<18;++i)for(unsigned c=0;c<3;++c)energy+=display.rgb[i][c];
        assert(brightness?energy>previous:energy==0);previous=energy;
        core_engine_display(b,&unrelated);assert(!memcmp(original.rgb,unrelated.rgb,sizeof original.rgb));
        if(brightness==50)assert(!memcmp(original.rgb,display.rgb,sizeof original.rgb));
    }
    state.brightness=0;assert(core_engine_update_settings(a,&state));
    state.brightness=50;assert(core_engine_update_settings(a,&state));
    core_engine_display(a,&display);assert(!memcmp(original.rgb,display.rgb,sizeof original.rgb));
    core_engine_destroy(a);core_engine_destroy(b);
    puts("engine: held LED brightness updates immediately at 0/25/50/75/100%, restores colors, isolates instances");
}
static void check_position_led(void) {
    CoreEngine *e=create(901);
    CoreControls controls={.knobs={0,0,0,.5f,0}};
    core_engine_controls(e,&controls);
    int16_t pcm[2];CoreDisplay display;
    // Startup and knob feedback must relinquish the ring promptly.
    for(unsigned f=0;f<CORE_RATE*2;++f)core_engine_process(e,pcm);
    unsigned slices=0,matched=0,checked=0;
    for(unsigned f=0;f<CORE_RATE*3;++f){
        core_engine_process(e,pcm);
        if(f%441!=100)continue;
        // The slice can change between the millisecond LED refreshes.
        core_engine_display(e,&display);
        unsigned led=display.slice%16;
        unsigned cyan=0;
        for(unsigned i=0;i<16;++i)if(display.rgb[i][0]==0&&display.rgb[i][1]>0&&display.rgb[i][2]>display.rgb[i][1]){cyan|=1u<<i;slices|=1u<<i;}
        assert(cyan);matched+=(cyan&(1u<<led))!=0;++checked;
    }
    assert((slices&(slices-1))!=0);
    assert(matched*100>checked*95);
    // A new knob overlay must also release back to position display.
    controls.knobs[0]=.75f;core_engine_controls(e,&controls);
    for(unsigned f=0;f<CORE_RATE/4;++f)core_engine_process(e,pcm);
    core_engine_display(e,&display);
    unsigned colored=0;for(unsigned i=0;i<16;++i)colored+=display.rgb[i][0]>0;
    assert(colored>0);
    for(unsigned f=0;f<CORE_RATE*2;++f)core_engine_process(e,pcm);
    core_engine_display(e,&display);
    assert(display.rgb[display.slice%16][0]==0&&display.rgb[display.slice%16][2]>0);
    core_engine_destroy(e);
    puts("engine: position LED returns after startup/knob feedback and follows playback slices");
}
static void check_tap_break_drive(void) {
    enum {SATURATE=0,FUZZ=2,TIMESTRETCH=4};
    CoreEngine *e=create(901);int16_t pcm[2];
    CoreControls controls={.knobs={.25f,0,0,.5f,0}};
    core_engine_controls(e,&controls);
    for(unsigned f=0;f<CORE_RATE;++f)core_engine_process(e,pcm);
    CoreState state;core_engine_get_state(e,&state);
    memset(state.runes,0,sizeof state.runes);
    state.runes[state.rune][TIMESTRETCH]=true;
    memset(state.effects,0,sizeof state.effects);
    state.volume=122;assert(core_engine_set_state(e,&state));
    for(unsigned f=0;f<CORE_RATE/4;++f)core_engine_process(e,pcm);
    controls.buttons[3]=true;core_engine_controls(e,&controls);
    for(unsigned f=0;f<CORE_RATE/10;++f)core_engine_process(e,pcm);
    const float values[]={0,.35f,.65f,.95f,.35f};
    for(unsigned step=0;step<sizeof values/sizeof *values;++step){
        controls.knobs[0]=values[step];core_engine_controls(e,&controls);
        for(unsigned f=0;f<CORE_RATE/4;++f)core_engine_process(e,pcm);
        core_engine_get_state(e,&state);
        assert(state.runes[state.rune][TIMESTRETCH]);
        if(values[step]<=.52f){
            assert(!state.effects[SATURATE]&&!state.effects[FUZZ]);
            assert(values[step]==0?state.volume==0:state.volume>0&&state.volume<244);
        } else {
            assert(state.volume==244);
            assert(state.effects[SATURATE]==(values[step]<.75f));
            assert(state.effects[FUZZ]==(values[step]>.75f));
        }
    }
    unsigned volume=state.volume;
    controls.buttons[3]=false;core_engine_controls(e,&controls);
    for(unsigned f=0;f<CORE_RATE/10;++f)core_engine_process(e,pcm);
    controls.knobs[0]=.8f;core_engine_controls(e,&controls);
    for(unsigned f=0;f<CORE_RATE/4;++f)core_engine_process(e,pcm);
    core_engine_get_state(e,&state);
    assert(state.volume==volume&&!state.effects[SATURATE]&&!state.effects[FUZZ]);
    core_engine_destroy(e);
    puts("engine: TAP + BREAK adjusts volume/saturation/fuzz on the time-stretch-only rune; unshifted BREAK preserves drive");
}
static void check_start_tempo(void) {
    CoreEngine *a=create(901),*b=create(902);
    CoreState state,after,other;core_engine_get_state(a,&state);core_engine_get_state(b,&other);
    unsigned original=state.tempo;assert(state.start_tempo==0);
    for(unsigned bpm=30;bpm<=300;++bpm) {
        state.start_tempo=bpm;assert(core_engine_update_settings(a,&state));
        core_engine_get_state(a,&after);assert(after.tempo==original&&after.start_tempo==bpm);
    }
    for(unsigned invalid=1;invalid<30;++invalid){state.start_tempo=invalid;assert(!core_engine_update_settings(a,&state));assert(!core_engine_set_state(a,&state));}
    state.start_tempo=301;assert(!core_engine_update_settings(a,&state));assert(!core_engine_set_state(a,&state));
    state.start_tempo=130;assert(core_engine_update_settings(a,&state));
    core_engine_apply_start_tempo(a);core_engine_get_state(a,&state);assert(state.tempo==130);
    core_engine_get_state(b,&after);assert(after.tempo==other.tempo&&after.start_tempo==0);
    state.tempo=145;assert(core_engine_set_state(a,&state));
    core_engine_set_resident_bank(a,0);core_engine_get_state(a,&state);assert(state.tempo==145);
    CoreControls controls={.knobs={0,0,0,.5f,0}};int16_t pcm[2];
    // Warm the real input handler, then tap a steady 120 BPM.
    for(unsigned f=0;f<CORE_RATE*2;++f)core_engine_process(a,pcm);
    for(unsigned f=0;f<CORE_RATE*3;++f){controls.buttons[3]=(f%(CORE_RATE/2))<2205;core_engine_controls(a,&controls);core_engine_process(a,pcm);}
    core_engine_get_state(a,&state);assert(state.tempo==120&&state.start_tempo==130);
    controls.buttons[3]=false;core_engine_controls(a,&controls);
    for(unsigned f=0;f<CORE_RATE;++f)core_engine_process(a,pcm);
    state.tempo=145;assert(core_engine_set_state(a,&state));
    // TAP + MODE still recalls the fixture sample's original 120 BPM.
    controls.buttons[3]=true;core_engine_controls(a,&controls);
    for(unsigned f=0;f<4410;++f)core_engine_process(a,pcm);
    controls.buttons[0]=true;core_engine_controls(a,&controls);
    for(unsigned f=0;f<4410;++f)core_engine_process(a,pcm);
    core_engine_get_state(a,&state);assert(state.tempo==120&&state.start_tempo==130);
    memset(controls.buttons,0,sizeof controls.buttons);core_engine_controls(a,&controls);
    core_engine_apply_start_tempo(a);
    for(unsigned f=0;f<CORE_RATE*6;++f){core_engine_clock(a,f%11025<200);core_engine_process(a,pcm);}
    core_engine_get_state(a,&state);assert(state.tempo==120&&state.start_tempo==130);
    state.start_tempo=0;assert(core_engine_update_settings(a,&state));core_engine_apply_start_tempo(a);
    core_engine_get_state(a,&after);assert(after.tempo==120&&after.start_tempo==0);
    core_engine_destroy(a);core_engine_destroy(b);
    puts("engine: startup override, bounds, independent instances, tap, sample-tempo reset and external clock passed");
}
int main(void) {
    check_led_brightness();
    check_position_led();
    check_tap_break_drive();
    check_start_tempo();
    CoreEngine *a=create(123),*b=create(123),*other=create(789);
    int16_t x[2],y[2],z[2];unsigned audible=0;
    for(unsigned frame=0;frame<CORE_RATE*3;++frame) {
        core_engine_process(a,x);core_engine_process(other,z);core_engine_process(b,y);
        assert(x[0]==y[0]&&x[1]==y[1]);audible+=x[0]!=0;
    }
    assert(audible>1000);
    // Exercise all original effect update/render paths, edge clocking, and
    // button/CV changes while an unrelated instance runs between every frame.
    for(unsigned effect=0;effect<16;++effect){
        CoreState sa,sb;core_engine_get_state(a,&sa);core_engine_get_state(b,&sb);
        memset(sa.effects,0,sizeof sa.effects);memset(sb.effects,0,sizeof sb.effects);
        sa.effects[effect]=sb.effects[effect]=true;
        assert(core_engine_set_state(a,&sa)&&core_engine_set_state(b,&sb));
        CoreControls c={.knobs={.7f,.4f,.3f,.5f,0},.cv={1,-1,0},.connected={true,true,false}};
        for(unsigned f=0;f<CORE_RATE/2;++f){
            c.buttons[0]=effect%4==0&&f<3000;c.buttons[1]=effect%4==1&&f<3000;
            c.buttons[3]=effect%4==2&&f<3000;
            core_engine_controls(a,&c);core_engine_controls(b,&c);
            bool clock=f%11025<200;core_engine_clock(a,clock);core_engine_clock(b,clock);
            core_engine_process(a,x);core_engine_process(other,z);core_engine_process(b,y);
            assert(x[0]==y[0]&&x[1]==y[1]);
        }
    }
    CoreState before,edit,after;core_engine_get_state(a,&before);edit=before;
    edit.brightness=25;edit.tempo=200;edit.random_state=0;
    assert(core_engine_update_settings(a,&edit));core_engine_get_state(a,&after);
    assert(after.random_state==before.random_state&&after.tempo==before.tempo&&after.brightness==25);
    CoreState s;core_engine_get_state(a,&s);assert(s.version==1);
    s.tempo=140;s.rune=3;s.stopped=true;assert(core_engine_set_state(a,&s));
    CoreState r;core_engine_get_state(a,&r);assert(r.tempo==140&&r.stopped&&r.rune==3);
    s.tempo=0;assert(!core_engine_set_state(a,&s));
    s.tempo=120;s.pitch=255;assert(!core_engine_set_state(a,&s));
    ThreadTrace t1={create(555),0},t2={create(555),0};pthread_t p1,p2;
    assert(!pthread_create(&p1,NULL,thread_trace,&t1)&&!pthread_create(&p2,NULL,thread_trace,&t2));
    pthread_join(p1,NULL);pthread_join(p2,NULL);assert(t1.hash==t2.hash);
    // Move each instance back to the main thread without resetting its state.
    thread_trace(&t1);thread_trace(&t2);assert(t1.hash==t2.hash);
    core_engine_destroy(t1.engine);core_engine_destroy(t2.engine);
    core_engine_destroy(a);core_engine_destroy(b);core_engine_destroy(other);
    puts("engine: 16 effects, CV/buttons/clock, deterministic PCM, independent instances, state/settings passed");
}
