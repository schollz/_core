// Copyright 2026 Zack Scholl, GPLv3.0
#include "bank_metadata.h"
#include "ff.h"
#include <limits.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

SampleList *banks[16];
static SampleList catalogue[16];
static uint8_t *arena;
static size_t capacity, used;
static _Atomic unsigned resident = 16, generation;
volatile MetadataStatus metadata_status = {.resident_bank=16,.loading_bank=16,
                                           .minimum_free_heap=UINT32_MAX};
// All scratch is fixed size and shared between the boot scan and foreground
// loader. No sample-sized stack arrays and no retained scan slice arrays.
static struct Parser {
    SampleInfo info;
    uint32_t pos, length, hash, word;
    uint32_t starts[255];
    uint16_t counts[3];
    uint8_t header[11];
    unsigned lane, entry;
    SampleInfo *view;
    bool bad;
} parser;
static uint8_t scratch[512];
static FIL file;
static bool opened;
static struct {
    unsigned bank, sample, stage;
    uint32_t started;
    bool failed, active;
    SampleInfo views[16];
} loader;
static SampleInfo scanned[16];

static size_t aligned(size_t n) { return (n+sizeof(void*)-1)&~(sizeof(void*)-1); }
static uint32_t le32(const uint8_t *p) {
    return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
static void error(unsigned e) { metadata_status.last_error=e; }
static void clear_details(SampleInfo *s) {
    s->slice_start=NULL;s->slice_stop=NULL;s->slice_type=NULL;s->transients=NULL;
}
static void invalidate(void) {
    atomic_store_explicit(&resident,16,memory_order_release);
    metadata_status.resident_bank=16;
    for(unsigned b=0;b<16;++b)for(unsigned s=0;s<catalogue[b].num_samples;++s)
        clear_details(catalogue[b].sample[s].snd[0]);
    used=0;metadata_status.arena_used=0;
}
bool metadata_ready(unsigned bank) {
    return bank<16 && atomic_load_explicit(&resident,memory_order_acquire)==bank;
}
unsigned metadata_generation(void) {return atomic_load_explicit(&generation,memory_order_acquire);}
bool metadata_selection_valid(unsigned b,unsigned s) {
    return b<16 && banks[b] && s<banks[b]->num_samples;
}
uint8_t metadata_filename_index(unsigned b,unsigned s) {
    return metadata_selection_valid(b,s)?banks[b]->sample[s].snd[0]->filename_index:0;
}
int metadata_ordinal(unsigned b,unsigned slot) {
    if(b>=16||!banks[b])return -1;
    for(unsigned s=0;s<banks[b]->num_samples;++s)
        if(metadata_filename_index(b,s)==slot)return (int)s;
    return -1;
}
static void path(char *p,unsigned b,unsigned slot) {
    snprintf(p,32,"bank%u/%u.0.wav.info",b+1,slot);
}
static void parser_begin(uint32_t length,SampleInfo *view) {
    memset(&parser,0,sizeof parser);
    parser.length=length;parser.hash=2166136261u;parser.view=view;
}
static bool header(void) {
    SampleInfo *s=&parser.info;
    s->size=le32(parser.header);uint32_t f=le32(parser.header+4);
    s->bpm=f&511;s->play_mode=(f>>9)&7;s->one_shot=(f>>12)&1;
    s->tempo_match=(f>>13)&1;s->oversampling=(f>>14)&1;s->num_channels=(f>>15)&1;
    s->version=(f>>16)&127;s->reserved=f>>23;
    unsigned splice=parser.header[8]|parser.header[9]<<8;
    s->splice_trigger=splice&32767;s->splice_variable=splice>>15;
    s->slice_num=parser.header[10];
    if(!s->size||s->size>INT32_MAX||!s->slice_num||s->version>1||
       s->splice_trigger<2||s->play_mode>4) {error(META_FORMAT);return false;}
    if(s->bpm<30)s->bpm=30;
    if(s->bpm>300)s->bpm=300;
    uint32_t min=11+9u*s->slice_num+(s->version?6:0);
    if(parser.length<min) {error(META_TRUNCATED);return false;}
    if(parser.view && (s->slice_num!=parser.view->slice_num ||
                       s->version!=parser.view->version)) {error(META_CHANGED);return false;}
    return true;
}
static bool feed(const uint8_t *p,unsigned n) {
    SampleInfo *s=&parser.info,*v=parser.view;
    for(unsigned k=0;k<n;++k) {
        uint8_t byte=p[k];unsigned pos=parser.pos++;
        parser.hash=(parser.hash^byte)*16777619u;
        if(pos<11) {
            parser.header[pos]=byte;
            if(pos==10&&!header())return false;
            continue;
        }
        pos-=11;unsigned count=s->slice_num;
        if(pos<count*8) {
            parser.word|=(uint32_t)byte<<((pos%4)*8);
            if(pos%4==3) {
                unsigned i=(pos/4)%count;uint32_t x=parser.word;parser.word=0;
                if(x>s->size) {error(META_RANGE);return false;}
                if(pos<count*4) {
                    parser.starts[i]=x;
                    if(v)v->slice_start[i]=(int32_t)x;
                } else {
                    if(x<=parser.starts[i]) {error(META_RANGE);return false;}
                    if(v)v->slice_stop[i]=(int32_t)x;
                }
            }
        } else if(pos<count*9) {
            if(v)v->slice_type[pos-count*8]=(int8_t)byte;
        } else {
            pos-=count*9;
            if(!s->version) {error(META_FORMAT);return false;}
            if(pos<6) {
                parser.counts[pos/2]|=(uint16_t)byte<<((pos%2)*8);
                if(pos==5) {
                    uint32_t expected=17+9u*count;
                    for(unsigned i=0;i<3;++i)expected+=2u*parser.counts[i];
                    if(expected!=parser.length) {error(META_TRUNCATED);return false;}
                    s->transient_num_1=parser.counts[0]>16?16:parser.counts[0];
                    s->transient_num_2=parser.counts[1]>16?16:parser.counts[1];
                    s->transient_num_3=parser.counts[2]>16?16:parser.counts[2];
                    if(v&&(s->transient_num_1!=v->transient_num_1||
                           s->transient_num_2!=v->transient_num_2||
                           s->transient_num_3!=v->transient_num_3)) {
                        error(META_CHANGED);return false;
                    }
                }
            } else {
                while(parser.lane<3 && parser.entry>=parser.counts[parser.lane]) {
                    ++parser.lane;parser.entry=0;
                }
                if(parser.lane>=3) {error(META_FORMAT);return false;}
                parser.word|=(uint32_t)byte<<(((pos-6)%2)*8);
                if((pos-6)%2) {
                    // Consume ALL on-disk values, retain only the first 16.
                    if(v&&parser.entry<16)v->transients[parser.lane][parser.entry]=(uint16_t)parser.word;
                    parser.word=0;++parser.entry;
                }
            }
        }
    }
    return true;
}
static bool finish(void) {
    SampleInfo *s=&parser.info;
    if(parser.pos!=parser.length||parser.pos<11||!s->slice_num) {error(META_TRUNCATED);return false;}
    if(!s->version&&parser.length!=11+9u*s->slice_num) {error(META_FORMAT);return false;}
    s->metadata_bytes=parser.length;s->metadata_checksum=parser.hash;
    size_t bytes=aligned(s->slice_num*4u)*2+aligned(s->slice_num);
    if(s->version)bytes+=aligned(3*sizeof(uint16_t*))+aligned(2u*s->transient_num_1)+
        aligned(2u*s->transient_num_2)+aligned(2u*s->transient_num_3);
    s->detail_bytes=bytes;
    return true;
}
void metadata_catalogue_destroy(void) {
    metadata_load_cancel();invalidate();free(arena);arena=NULL;capacity=0;
    for(unsigned b=0;b<16;++b) {
        free(catalogue[b].sample);memset(&catalogue[b],0,sizeof catalogue[b]);
        banks[b]=&catalogue[b];
    }
    metadata_status.arena_capacity=0;
}
bool metadata_catalogue_scan(void) {
    metadata_catalogue_destroy();bool ok=true;
    for(unsigned b=0;b<16;++b) {
        unsigned count=0;uint16_t slots=0;size_t bytes=0;
        for(unsigned slot=0;slot<16;++slot) {
            char name[32];path(name,b,slot);
            FRESULT r=f_open(&file,name,FA_READ);
            if(r==FR_NO_FILE||r==FR_NO_PATH)continue;
            if(r!=FR_OK) {error(META_IO);++metadata_status.rejected_files;continue;}
            bool valid=f_size(&file)<=UINT32_MAX;
            parser_begin((uint32_t)f_size(&file),NULL);
            while(valid&&parser.pos<parser.length) {
                UINT n=parser.length-parser.pos;if(n>512)n=512;
                UINT got=0;r=f_read(&file,scratch,n,&got);
                if(r!=FR_OK||got!=n) {error(r?META_IO:META_TRUNCATED);valid=false;}
                else valid=feed(scratch,n);
            }
            valid=valid&&finish();
            if(f_close(&file)!=FR_OK) {valid=false;error(META_IO);}
            // Metadata size excludes the padded audio wrapper. It must at least
            // fit in the physical file. Variant files keep the existing format.
            if(valid) {
                snprintf(name,sizeof name,"bank%u/%u.0.wav",b+1,slot);
                FILINFO info;r=f_stat(name,&info);
                valid=r==FR_OK && info.fsize>=parser.info.size+44u;
                if(!valid)error(META_RANGE);
            }
            if(!valid) {++metadata_status.rejected_files;continue;}
            scanned[count]=parser.info;scanned[count].filename_index=slot;
            bytes+=scanned[count].detail_bytes;slots|=1u<<slot;++count;
        }
        SampleList *list=&catalogue[b];
        if(!count)continue;
        // One allocation per bank; entries and mutable settings survive reloads.
        size_t offset=aligned(count*sizeof(Sample));
        list->sample=calloc(1,offset+count*sizeof(SampleInfo));
        if(!list->sample) {ok=false;error(META_MEMORY);break;}
        SampleInfo *infos=(SampleInfo*)((uint8_t*)list->sample+offset);
        for(unsigned s=0;s<count;++s) {
            infos[s]=scanned[s];list->sample[s].snd[0]=&infos[s];
        }
        list->num_samples=count;list->valid_slots=slots;list->detail_bytes=bytes;
        if(bytes>capacity)capacity=bytes;
    }
    if(!ok)metadata_catalogue_destroy();
    return ok;
}
bool metadata_reserve(void) {
    if(arena)return true;
    if(!capacity) {error(META_MEMORY);return false;}
    arena=malloc(capacity);
    if(!arena) {error(META_MEMORY);return false;}
    metadata_status.arena_capacity=capacity;return true;
}
static void *take(size_t n) {
    n=aligned(n);if(n>capacity-used)return NULL;
    void *p=arena+used;used+=n;return p;
}
bool metadata_load_begin(unsigned bank,uint32_t now) {
    if(opened||!metadata_selection_valid(bank,0)) {error(META_FORMAT);return false;}
    if(!arena) {error(META_MEMORY);return false;}
    invalidate();memset(&loader,0,sizeof loader);loader.bank=bank;loader.started=now;loader.active=true;
    metadata_status.loading_bank=bank;
    for(unsigned i=0;i<banks[bank]->num_samples;++i) {
        SampleInfo *v=&loader.views[i];*v=*banks[bank]->sample[i].snd[0];
        if(v->detail_bytes>capacity-used) {error(META_MEMORY);return false;}
        v->slice_start=take(4u*v->slice_num);v->slice_stop=take(4u*v->slice_num);
        v->slice_type=take(v->slice_num);
        if(v->version) {
            v->transients=take(3*sizeof(uint16_t*));
            v->transients[0]=take(2u*v->transient_num_1);
            v->transients[1]=take(2u*v->transient_num_2);
            v->transients[2]=take(2u*v->transient_num_3);
        }
    }
    return true;
}
int metadata_load_step(uint32_t now) {
    if(!loader.active||loader.failed)return -1;
    if((uint32_t)(now-loader.started)>2000000u) {error(META_TIMEOUT);loader.failed=true;return -1;}
    if(loader.sample>=banks[loader.bank]->num_samples)return 1;
    SampleInfo *v=&loader.views[loader.sample];
    FRESULT r;
    if(loader.stage==0) {
        char name[32];path(name,loader.bank,v->filename_index);
        r=f_open(&file,name,FA_READ);
        if(r!=FR_OK) {error(META_IO);loader.failed=true;return -1;}
        opened=true;
        if(f_size(&file)!=v->metadata_bytes) {error(META_CHANGED);loader.failed=true;return -1;}
        parser_begin(v->metadata_bytes,v);loader.stage=1;
    } else if(loader.stage==1) {
        UINT n=parser.length-parser.pos;if(n>512)n=512;UINT got=0;
        r=f_read(&file,scratch,n,&got);
        if(r!=FR_OK||got!=n) {error(r?META_IO:META_TRUNCATED);loader.failed=true;return -1;}
        if(!feed(scratch,n)) {loader.failed=true;return -1;}
        if(parser.pos==parser.length) {
            if(!finish()||parser.hash!=v->metadata_checksum||parser.info.detail_bytes!=v->detail_bytes) {
                error(META_CHANGED);loader.failed=true;return -1;
            }
            loader.stage=2;
        }
    } else {
        r=f_close(&file);opened=false;
        if(r!=FR_OK) {error(META_IO);loader.failed=true;return -1;}
        ++loader.sample;loader.stage=0;
    }
    return 0;
}
void metadata_load_cancel(void) {
    if(opened) {f_close(&file);opened=false;}
    memset(&loader,0,sizeof loader);metadata_status.loading_bank=16;
}
void metadata_publish(void) {
    if(!loader.active||loader.failed||opened||loader.sample!=banks[loader.bank]->num_samples)return;
    loader.active=false;
    for(unsigned i=0;i<loader.sample;++i) {
        SampleInfo *to=banks[loader.bank]->sample[i].snd[0],*from=&loader.views[i];
        to->slice_start=from->slice_start;to->slice_stop=from->slice_stop;
        to->slice_type=from->slice_type;to->transients=from->transients;
    }
    unsigned g=atomic_load_explicit(&generation,memory_order_relaxed)+1;
    atomic_store_explicit(&generation,g,memory_order_release);
    atomic_store_explicit(&resident,loader.bank,memory_order_release);
    metadata_status.resident_bank=loader.bank;metadata_status.generation=g;
    metadata_status.arena_used=used;metadata_status.loading_bank=16;
}
