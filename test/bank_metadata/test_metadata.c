#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "ff.h"
static unsigned allocations,live,fail_alloc,operations,open_files;
static int read_fail_bank=-1,short_bank=-1,open_fail_bank=-1;
static void *test_malloc(size_t n) {
    if(++allocations==fail_alloc)return NULL;
    void *p=malloc(n);if(p)++live;return p;
}
static void *test_calloc(size_t n,size_t s) {
    void *p=test_malloc(n*s);if(p)memset(p,0,n*s);return p;
}
static void test_free(void *p) {if(p) {assert(live);--live;}free(p);}
#define malloc test_malloc
#define calloc test_calloc
#define free test_free
#include "../../lib/bank_metadata.c"
#undef malloc
#undef calloc
#undef free
static struct Fixture {uint8_t data[4096];unsigned size;bool exists;} files[16][16];
static void put(unsigned b,unsigned s,unsigned pos,uint32_t x,unsigned n) {
    for(unsigned i=0;i<n;++i)files[b][s].data[pos+i]=x>>(i*8);
}
static void fixture(unsigned b,unsigned s,unsigned slices,unsigned excess) {
    struct Fixture *f=&files[b][s];memset(f,0,sizeof *f);f->exists=true;
    put(b,s,0,slices*4096,4);put(b,s,4,170u|(1u<<13)|(1u<<16),4);
    put(b,s,8,96,2);put(b,s,10,slices,1);
    for(unsigned i=0;i<slices;++i) {put(b,s,11+4*i,i*4096,4);put(b,s,11+4*slices+4*i,(i+1)*4096,4);}
    unsigned pos=11+9*slices;
    put(b,s,pos,excess,2);put(b,s,pos+2,2,2);put(b,s,pos+4,1,2);pos+=6;
    for(unsigned i=0;i<excess+3;++i) {put(b,s,pos,17+i,2);pos+=2;}
    f->size=pos;
}
FRESULT f_open(FIL *f,const char *path,unsigned flags) {
    (void)flags;++operations;unsigned b,s;
    assert(sscanf(path,"bank%u/%u.0.wav.info",&b,&s)==2);assert(b&&b<=16&&s<16);--b;
    if((int)b==open_fail_bank)return FR_DISK_ERR;
    if(!files[b][s].exists)return FR_NO_FILE;
    *f=(FIL){.bank=b,.slot=s,.size=files[b][s].size,.open=1};++open_files;return FR_OK;
}
FRESULT f_read(FIL *f,void *out,UINT n,UINT *got) {
    ++operations;assert(f->open&&n<=512);
    if((int)f->bank==read_fail_bank) {*got=0;return FR_DISK_ERR;}
    *got=n;if((int)f->bank==short_bank&&n)--*got;
    assert(f->pos+*got<=files[f->bank][f->slot].size);
    memcpy(out,files[f->bank][f->slot].data+f->pos,*got);f->pos+=*got;return FR_OK;
}
FRESULT f_close(FIL *f) {++operations;assert(f->open);f->open=0;--open_files;return FR_OK;}
FRESULT f_stat(const char *name,FILINFO *info) {(void)name;++operations;info->fsize=10000000;return FR_OK;}
static int load(unsigned b) {
    assert(metadata_load_begin(b,0));int result;uint32_t now=0;
    do {unsigned before=operations;result=metadata_load_step(now+=100);assert(operations-before<=1);}while(!result);
    return result;
}
static void setup(unsigned bank_count) {
    metadata_catalogue_destroy();assert(!live&&!open_files);
    memset(files,0,sizeof files);fail_alloc=0;read_fail_bank=short_bank=open_fail_bank=-1;
    for(unsigned b=0;b<bank_count;++b)for(unsigned s=0;s<16;++s)fixture(b,s,128,20);
}
static void parser_tests(void) {
    setup(16);assert(metadata_catalogue_scan());assert(live==16);
    assert(metadata_reserve());assert(live==17);
    unsigned count=allocations;
    for(unsigned turn=0;turn<64;++turn) {
        unsigned b=turn%16;assert(load(b)==1);assert(!metadata_ready(b));metadata_publish();
        for(unsigned other=0;other<16;++other) {
            SampleInfo *s=banks[other]->sample[0].snd[0];
            assert(metadata_ready(other)==(other==b));
            assert((s->slice_start!=NULL)==(other==b));
        }
        SampleInfo *s=banks[b]->sample[0].snd[0];
        assert(s->transient_num_1==16&&s->transients[1][0]==37&&s->transients[2][0]==39);
        assert(s->slice_start[127]==127*4096&&s->slice_stop[127]==128*4096);
        s->bpm=99;s->play_mode=2;s->tempo_match=0;
    }
    assert(allocations==count&&live==17&&!open_files);
    assert(load(0)==1);metadata_publish();assert(banks[0]->sample[0].snd[0]->bpm==99);
    printf("16 banks / 256 samples: arena %u bytes; repeated switches allocate nothing\n",metadata_status.arena_capacity);
    setup(1);files[0][1].exists=false;files[0][4].data[10]=0;
    assert(metadata_catalogue_scan());assert(banks[0]->num_samples==14);
    assert(metadata_filename_index(0,1)==2&&metadata_ordinal(0,1)==-1&&metadata_ordinal(0,4)==-1);
    assert(banks[0]->valid_slots==0xffed);assert(metadata_reserve());
    files[0][2].data[11]^=1;assert(load(0)<0);assert(metadata_status.last_error==META_CHANGED);
    metadata_load_cancel();assert(!open_files&&!metadata_ready(0));
    // Every truncation boundary, malformed version/count/range, short/error IO.
    setup(1);for(unsigned i=1;i<16;++i)files[0][i].exists=false;
    unsigned length=files[0][0].size;
    for(unsigned n=0;n<length;++n) {
        files[0][0].size=n;assert(metadata_catalogue_scan());assert(!banks[0]->num_samples);
    }
    fixture(0,0,2,0);put(0,0,4,170|(2<<16),4);assert(metadata_catalogue_scan());assert(!banks[0]->num_samples);
    fixture(0,0,2,0);put(0,0,11+8,0,4);assert(metadata_catalogue_scan());assert(!banks[0]->num_samples);
    fixture(0,0,2,0);put(0,0,8,1,2);assert(metadata_catalogue_scan());assert(!banks[0]->num_samples);
    fixture(0,0,2,0);put(0,0,4,170,4);files[0][0].size=29;
    assert(metadata_catalogue_scan());assert(metadata_reserve());assert(load(0)==1);metadata_publish();
    assert(!banks[0]->sample[0].snd[0]->transients);
    fixture(0,0,2,0);assert(metadata_catalogue_scan());assert(metadata_reserve());
    short_bank=0;assert(load(0)<0);metadata_load_cancel();short_bank=-1;
    read_fail_bank=0;assert(load(0)<0);metadata_load_cancel();read_fail_bank=-1;
    assert(metadata_load_begin(0,0));assert(metadata_load_step(2000001)<0);metadata_load_cancel();
    // All catalogue and arena allocation failures leave initialized objects.
    for(unsigned i=1;i<=17;++i) {
        setup(16);fail_alloc=allocations+i;
        bool scanned_ok=metadata_catalogue_scan();
        if(i<17)assert(!scanned_ok);else {assert(scanned_ok);assert(!metadata_reserve());}
        metadata_catalogue_destroy();assert(!live&&!open_files);
    }
}

// Run the actual foreground transition service with fake audio ownership.
static uint32_t now;
static uint32_t time_us_32(void) {return now;}
static uint32_t getFreeHeap(void) {return 64000;}
static bool fil_is_open,fil_current_change,fil_current_change_force,do_open_file_ready;
static bool audio_was_muted,first_loop_ever,phase_change,mute_because_of_playback_type;
static bool button_mute,playback_stopped,phase_forward;
static uint8_t sel_bank_cur,sel_bank_next,sel_sample_cur,sel_sample_next,sel_variation,audio_variant;
static int32_t phases[2],phase_new,beat_current;
static float sel_variation_scale[2]={1,0.5};
static _Atomic unsigned audio_media_recovery;
static unsigned ownership,resets;
static int playback_fail_bank=-1;
static bool audio_media_acquire(void) {assert(!ownership);++ownership;return true;}
static void audio_media_release(void) {assert(ownership==1);--ownership;}
static void realtime_stretch_reset_from_playback_phase(void) {++resets;}
static void format_sample_filename(char *p,unsigned b,unsigned s,unsigned v) {
    snprintf(p,32,"bank%u/%u.%u.wav",b+1,metadata_filename_index(b,s),v);
}
static FRESULT audio_file_close(void) {assert(ownership);if(fil_is_open)++operations;fil_is_open=false;return FR_OK;}
static FRESULT audio_file_open(const char *name) {
    assert(ownership);audio_file_close();++operations;unsigned bank;assert(sscanf(name,"bank%u/",&bank)==1);
    if((int)(bank-1)==playback_fail_bank)return FR_DISK_ERR;
    fil_is_open=true;return FR_OK;
}
#include "bank_transition.h"
#include "bank_transition_impl.h"
static void step(void) {
    now+=1000;unsigned before=operations;bank_transition_service();assert(operations-before<=1);
    if(bank_transition_fading()) {
        atomic_store(&bank_fade_silent,playback_stopped||mute_because_of_playback_type||button_mute);
        atomic_store(&bank_fade_done,true);
    }
}
static void drain(void) {for(unsigned i=0;i<2000 && (bank_transition_busy()||fil_current_change);++i)step();assert(!bank_transition_busy()&&!fil_current_change&&!ownership);}
static void request(unsigned b,unsigned s) {sel_bank_next=b;sel_sample_next=s;fil_current_change=true;}
static void transition_tests(void) {
    setup(7);assert(metadata_catalogue_scan()&&metadata_reserve());assert(load(0)==1);metadata_publish();
    sel_bank_cur=sel_bank_next=0;sel_sample_cur=sel_sample_next=0;fil_is_open=true;
    phases[0]=4096;phases[1]=2048;phase_new=400;beat_current=1;
    unsigned allocation_count=allocations;
    request(2,3);drain();assert(sel_bank_cur==2&&sel_sample_cur==3&&phases[0]==4096&&fil_is_open);
    button_mute=true;playback_stopped=true;phase_forward=false;mute_because_of_playback_type=true;
    request(3,4);drain();assert(sel_bank_cur==3&&!phases[0]&&!mute_because_of_playback_type);
    assert(button_mute&&playback_stopped&&!phase_forward);
    button_mute=playback_stopped=false;
    request(1,1);while(!ownership)step();step();step();request(6,15);drain();assert(sel_bank_cur==6&&sel_sample_cur==15);
    // Failed new metadata, successful rollback preserves exact phase/state.
    phases[0]=12345;phases[1]=234;phase_new=789;phase_change=true;beat_current=7;
    unsigned rollbacks=metadata_status.rollbacks;read_fail_bank=2;
    request(2,0);drain();assert(sel_bank_cur==6&&metadata_ready(6)&&fil_is_open);
    assert(phases[0]==12345&&phases[1]==234&&phase_new==789&&phase_change&&beat_current==7);
    assert(metadata_status.rollbacks==rollbacks+1);
    // A failed rollback remains silent, does not retry, accepts another choice.
    open_fail_bank=6;request(2,0);drain();assert(!fil_is_open&&!metadata_ready(6));
    unsigned before=operations;for(unsigned i=0;i<100;++i)step();assert(operations==before);
    read_fail_bank=open_fail_bank=-1;request(4,0);drain();assert(fil_is_open&&metadata_ready(4));
    playback_fail_bank=5;request(5,0);drain();assert(sel_bank_cur==4&&fil_is_open&&metadata_ready(4));
    playback_fail_bank=-1;
    // A new selection arriving during rollback is replayed after restoration.
    read_fail_bank=2;request(2,0);
    while(atomic_load(&bank_transition_state)!=BANK_ROLLBACK)step();
    request(1,9);drain();assert(sel_bank_cur==1&&sel_sample_cur==9&&metadata_ready(1));
    read_fail_bank=-1;
    // Preserve the existing scaled relative-phase behavior for variation 1.
    sel_variation=1;phases[0]=16000;request(3,2);drain();assert(phases[0]==4000);
    sel_variation=0;
    // Deadline expiration has one independent rollback deadline.
    request(4,0);while(atomic_load(&bank_transition_state)!=BANK_LOADING||bank_job.stage<2)step();
    now+=2000001;step();assert(atomic_load(&bank_transition_state)==BANK_ROLLBACK);
    drain();assert(sel_bank_cur==3&&metadata_ready(3));
    assert(allocations==allocation_count);
    metadata_catalogue_destroy();assert(!live&&!open_files);
}
int main(void) {parser_tests();transition_tests();puts("metadata parser, ownership, failure, memory and transition tests passed");}
