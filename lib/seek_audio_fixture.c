#include "seek_audio_fixture.h"
#if SEEK_TEST_AUDIO_FIXTURE
#include "seek_hash.h"
#include "audio_seek_map.h"
#include <stdbool.h>
#include <string.h>
#ifndef SEEK_MAP_HOST_TEST
#include "pico/time.h"
#define now_us() time_us_32()
#else
#define now_us() 0u
#endif
#define MAX_BYTES (8u*1024u*1024u)
seek_audio_fixture_report zeptocore_audio_fixture;
static uint32_t u32(const BYTE *p) { return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
static void p32(BYTE *p,uint32_t x) { for(unsigned i=0;i<4;++i)p[i]=x>>(i*8); }

FRESULT seek_audio_fixture_run(unsigned mode,const uint8_t cid[16]) {
    seek_audio_fixture_report *report=&zeptocore_audio_fixture;
    atomic_store_explicit(&report->complete,0,memory_order_release);
    report->mode=mode;report->operation=report->bytes=report->starting_cluster=report->reserved=0;
    uint32_t started=now_us();FRESULT r=FR_OK;FIL source={0},target={0};
    BYTE data[512],marker[72],digest[32];seek_sha256_t hash;
    UINT n,written;bool source_open=false,target_open=false;
    const char *source_path=mode>2?"bank1/0.1.wav":"bank1/0.0.wav";
    const char *target_path=mode>2?"bank1/0.3.wav":"bank1/0.2.wav";
    const char *marker_path=mode>2?".core_seek_bench/audio-variant1.bin":".core_seek_bench/audio-variant.bin";
    if(mode<1||mode>4||!cid){r=FR_INVALID_PARAMETER;goto done;}
    r=f_mkdir(".core_seek_bench");if(r&&r!=FR_EXIST)goto done;
    r=f_open(&source,marker_path,FA_READ);
    if(!r) {
        source_open=true;
        if(f_size(&source)!=sizeof marker){r=FR_INVALID_PARAMETER;goto done;}
        r=f_read(&source,marker,sizeof marker,&n);if(r)goto done;
        if(n!=sizeof marker||u32(marker)!=0x31464153u||u32(marker+4)!=1||
           memcmp(marker+8,cid,16)||u32(marker+28)||u32(marker+24)>MAX_BYTES||
           u32(marker+68)!=seek_crc32(marker,68)){r=FR_INVALID_PARAMETER;goto done;}
        r=f_close(&source);source_open=false;if(r)goto done;
        r=f_open(&target,target_path,FA_READ);if(r)goto done;target_open=true;
        if(f_size(&target)!=u32(marker+24)||target.obj.sclust!=u32(marker+32)) {
            r=FR_INVALID_PARAMETER;goto done;
        }
        seek_sha256_init(&hash);
        do {
            r=f_read(&target,data,sizeof data,&n);if(r)goto done;
            seek_sha256_update(&hash,data,n);
        } while(n);
        seek_sha256_final(&hash,digest);
        if(memcmp(digest,marker+36,32)){r=FR_INVALID_PARAMETER;goto done;}
        report->bytes=u32(marker+24);report->starting_cluster=u32(marker+32);
        r=f_close(&target);target_open=false;if(r)goto done;
        if(mode&1){report->operation=2;goto done;}
        if(!seek_maps_invalidate(target_path)){r=FR_LOCKED;goto done;}
        r=f_unlink(target_path);if(r)goto done;
        r=f_unlink(marker_path);if(!r)report->operation=3;
        goto done;
    }
    if(r!=FR_NO_FILE)goto done;
    if(!(mode&1)){r=FR_OK;report->operation=4;goto done;}
    r=f_open(&source,source_path,FA_READ);if(r)goto done;source_open=true;
    FSIZE_t expected=f_size(&source);
    if(!expected||expected>MAX_BYTES){r=FR_INVALID_PARAMETER;goto done;}
    // Never replace an existing physical variant, even without a marker.
    if(!seek_maps_invalidate(target_path)){r=FR_LOCKED;goto done;}
    r=f_open(&target,target_path,FA_WRITE|FA_CREATE_NEW);if(r)goto done;target_open=true;
    seek_sha256_init(&hash);
    while(f_tell(&source)<expected) {
        r=f_read(&source,data,sizeof data,&n);if(r)goto done;
        if(!n){r=FR_INT_ERR;goto done;}
        r=f_write(&target,data,n,&written);if(r)goto done;
        if(written!=n){r=FR_DENIED;goto done;}
        seek_sha256_update(&hash,data,n);
    }
    report->bytes=f_size(&target);report->starting_cluster=target.obj.sclust;
    r=f_close(&target);target_open=false;if(r)goto done;
    r=f_close(&source);source_open=false;if(r)goto done;
    seek_sha256_final(&hash,digest);
    memset(marker,0,sizeof marker);p32(marker,0x31464153u);p32(marker+4,1);
    memcpy(marker+8,cid,16);p32(marker+24,report->bytes);p32(marker+32,report->starting_cluster);
    memcpy(marker+36,digest,32);p32(marker+68,seek_crc32(marker,68));
    r=f_open(&source,marker_path,FA_WRITE|FA_CREATE_NEW);if(r)goto done;source_open=true;
    r=f_write(&source,marker,sizeof marker,&written);if(r)goto done;
    if(written!=sizeof marker){r=FR_DENIED;goto done;}
    r=f_close(&source);source_open=false;if(!r)report->operation=1;
done:
    if(target_open){FRESULT close=f_close(&target);if(!r)r=close;}
    if(source_open){FRESULT close=f_close(&source);if(!r)r=close;}
    report->result=r;report->elapsed_us=now_us()-started;
    atomic_store_explicit(&report->complete,0x31544641u,memory_order_release);
    return r;
}
#endif
