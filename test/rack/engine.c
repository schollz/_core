// Independent instances must produce the same PCM regardless of interleaving.
#include "core_engine.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
static bool read_pcm(void *u,unsigned bank,unsigned slot,unsigned variant,uint64_t offset,void *out,size_t n) {
    (void)u;(void)bank;(void)slot;
    int16_t *p=out;
    for(size_t i=0;i<n/2;++i) p[i]=(int16_t)(12000*sin(6.283185307179586*220*((offset+i*2)/4)/(44100*(variant?8:1))));
    return true;
}
static CoreEngine *create(uint64_t seed) {
    CoreEngine *e=core_engine_create(seed);assert(e);
    static const int32_t starts[]={0,44100,88200,132300},stops[]={44100,88200,132300,176400};
    static const int8_t types[]={0,0,0,0};
    static const uint16_t transients[]={0,689};
    CoreBank banks[16]={0};banks[0].count=1;
    banks[0].samples[0]=(CoreSample){.size=176400,.bpm=120,.splice_ticks=96,.channels=2,.rate_multiple=1,
        .tempo_match=true,.slice_count=4,.starts=starts,.stops=stops,.types=types,.transient_count={2,0,0},.transients={transients,0,0}};
    core_engine_set_catalogue(e,banks,read_pcm,0);core_engine_set_resident_bank(e,0);
    CoreControls c={.knobs={0,0,0,.5f,0}};core_engine_controls(e,&c);return e;
}
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
int main(void) {
    check_led_brightness();
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
