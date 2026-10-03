#ifndef ENGINE_FIXTURE_H
#define ENGINE_FIXTURE_H
#include "core_engine.h"
#include <assert.h>
#include <math.h>

static bool read_pcm(void *u,unsigned bank,unsigned slot,unsigned variant,uint64_t offset,void *out,size_t n) {
    (void)u;(void)bank;(void)slot;
    assert(variant==0); // Every effect, including time stretch, reads primary PCM.
    int16_t *p=out;
    for(size_t i=0;i<n/2;++i) p[i]=(int16_t)(12000*sin(6.283185307179586*220*((offset+i*2)/4)/44100));
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
#endif
