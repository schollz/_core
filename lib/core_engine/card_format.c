// Port of sample-manager/Source/CardFormat.cpp's card decoder.
// Copyright 2026 Zack Scholl. SPDX-License-Identifier: GPL-3.0-only
#include "card_format.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>
static uint16_t u16(const uint8_t *p){return p[0]|(uint16_t)p[1]<<8;}
static uint32_t u32(const uint8_t *p){return u16(p)|(uint32_t)u16(p+2)<<16;}
static bool bad(char *out,size_t n,const char *s){if(n)snprintf(out,n,"%s",s);return false;}
bool core_card_decode(const uint8_t *p,size_t n,CoreCardInfo *out,char *err,size_t cap) {
    if(!p||n<11)return bad(err,cap,"Truncated metadata");
    memset(out,0,sizeof *out);CoreSample *s=&out->sample;
    s->size=u32(p);uint32_t f=u32(p+4);unsigned version=(f>>16)&127;
    s->bpm=f&511;s->mode=(f>>9)&7;s->one_shot=(f>>12)&1;s->tempo_match=(f>>13)&1;
    s->rate_multiple=((f>>14)&1)+1;s->channels=((f>>15)&1)+1;
    s->splice_ticks=u16(p+8)&32767;s->variable=(u16(p+8)>>15)!=0;s->slice_count=p[10];
    if(!s->size||s->size>INT32_MAX||s->size%(s->channels*2)||f>>23||version>1||s->bpm>510||s->mode>4||s->splice_ticks<2||!s->slice_count)
        return bad(err,cap,"Invalid card metadata fields");
    size_t end=11+9u*s->slice_count;
    if(n<end)return bad(err,cap,"Truncated slices");
    for(unsigned i=0;i<s->slice_count;++i){
        uint32_t a=u32(p+11+i*4),b=u32(p+11+s->slice_count*4+i*4);
        if(a>=b||b>s->size||a%(s->channels*2)||b%(s->channels*2))return bad(err,cap,"Invalid slice boundaries");
        out->starts[i]=a;out->stops[i]=b;out->types[i]=(int8_t)p[11+s->slice_count*8+i];
    }
    if(version==1){
        if(n<end+6)return bad(err,cap,"Missing transient counts");
        unsigned count[3]={u16(p+end),u16(p+end+2),u16(p+end+4)};end+=6;
        for(unsigned lane=0;lane<3;++lane){
            if(count[lane]>16||n<end+2*count[lane])return bad(err,cap,"Invalid transient lane");
            s->transient_count[lane]=count[lane];
            for(unsigned i=0;i<count[lane];++i){out->transients[lane][i]=u16(p+end);end+=2;}
        }
    }
    if(end!=n)return bad(err,cap,"Unrecognized metadata extension");
    s->starts=out->starts;s->stops=out->stops;s->types=out->types;
    for(unsigned i=0;i<3;++i)s->transients[i]=out->transients[i];
    return true;
}
