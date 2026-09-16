#include "seek_fragment_benchmark.h"
#if SEEK_TEST_FRAGMENT_BENCH
#include "ff_clmt_validate.h"
#include <string.h>
#ifndef SEEK_MAP_HOST_TEST
#include "pico/time.h"
#define now_us() time_us_32()
#else
#define now_us() 0u
#endif
#define FIXTURE ".core_seek_bench/fragment.bin"
#define SPACING ".core_seek_bench/spacing.tmp"
#define FRAGMENTS 31u
#define PAIRS 768u
seek_benchmark_report zeptocore_seek_benchmark;
DWORD seek_benchmark_count_fat,seek_benchmark_fat_visits;
static BYTE pattern(uint32_t n) { return (BYTE)((n*31u)^(n>>9)); }

FRESULT seek_fragment_benchmark(FATFS *fs,FIL *file,FIL *spacing,DWORD table[64],BYTE data[512]) {
    if(atomic_load_explicit(&zeptocore_seek_benchmark.complete,memory_order_acquire))return FR_OK;
    seek_benchmark_report *report=&zeptocore_seek_benchmark;
    uint32_t *c=report->context,start=now_us(),cluster=fs->csize*512u,size=cluster*FRAGMENTS;
    bool file_open=false,spacing_open=false,spacing_created=false;
    FRESULT r=FR_OK;UINT n;
    c[0]=1;c[2]=fs->csize;c[3]=size;
    // These bounds also keep the temporary spacing file below 4 GiB.
    if(!cluster||cluster>65536u){r=FR_INVALID_PARAMETER;goto done;}
    r=f_mkdir(".core_seek_bench");if(r&&r!=FR_EXIST)goto done;
    r=f_open(file,FIXTURE,FA_READ);
    if(r==FR_NO_FILE) {
        // CREATE_NEW protects any pre-existing names, including interrupted
        // fixtures. Never truncate or overwrite an unknown file.
        r=f_open(spacing,SPACING,FA_WRITE|FA_CREATE_NEW);if(r)goto done;
        spacing_open=spacing_created=true;
        r=f_open(file,FIXTURE,FA_WRITE|FA_CREATE_NEW);if(r)goto done;
        file_open=true;c[7]=1;
        for(unsigned fragment=0;fragment<FRAGMENTS;++fragment) {
            for(unsigned offset=0;offset<cluster;offset+=512) {
                for(unsigned i=0;i<512;++i)data[i]=pattern(fragment*cluster+offset+i);
                r=f_write(file,data,512,&n);if(r||n!=512){if(!r)r=FR_DENIED;goto done;}
            }
            if(fragment+1<FRAGMENTS) {
                FSIZE_t end=(FSIZE_t)(fragment+1)*128*cluster;
                r=f_lseek(spacing,end);if(r||f_tell(spacing)!=end){if(!r)r=FR_DENIED;goto done;}
            }
        }
        r=f_close(file);file_open=false;if(r)goto done;
        r=f_close(spacing);spacing_open=false;if(r)goto done;
        r=f_unlink(SPACING);spacing_created=false;if(r)goto done;
        r=f_open(file,FIXTURE,FA_READ);
    }
    if(r)goto done;file_open=true;
    if(f_size(file)!=size){r=FR_INVALID_PARAMETER;goto done;}
    c[14]=file->obj.sclust;
    table[0]=64;file->cltbl=table;uint32_t build=now_us();
    r=f_lseek(file,CREATE_LINKMAP);c[8]=now_us()-build;file->cltbl=NULL;if(r)goto done;
    ff_clmt_info info;r=ff_clmt_inspect(file,table,table[0],&info);if(r)goto done;
    c[4]=info.fragments;c[5]=table[0];
    if(info.fragments!=FRAGMENTS||table[0]!=64){r=FR_INVALID_PARAMETER;goto done;}
    c[9]=now_us()-start;
    uint32_t rng=0x67c0de11u;
    for(unsigned pair=0;pair<PAIRS;++pair) {
        rng=rng*1664525u+1013904223u;
        uint32_t position;
        switch(pair%3) {
        case 0: position=size-512-(pair/3*512)%(size-512);break; // reverse
        case 1: position=44+(rng%(size-556));break; // slice/WAV offset
        default: position=(pair/3&1)?size-cluster+44:cluster+44;break; // distant grains
        }
        if(position>size-512)position=size-512;
        // Alternate ordering to balance caches. Begin each member at the same
        // endpoint, then time only the target seek and read separately.
        for(unsigned order=0;order<2;++order) {
            bool mapped=(pair+order)&1;
            file->cltbl=mapped?table:NULL;
            r=f_lseek(file,size);if(r)goto done;
            seek_benchmark_fat_visits=0;seek_benchmark_count_fat=1;
            uint32_t t=now_us();r=f_lseek(file,position);uint32_t us=now_us()-t;
            seek_benchmark_count_fat=0;c[10+mapped]+=seek_benchmark_fat_visits;
            zd_metric_record(mapped?&report->mapped_seek:&report->ordinary_seek,us,r,0,false,&c[13]);
            if(r||f_tell(file)!=position){if(!r)r=FR_INT_ERR;goto done;}
            t=now_us();r=f_read(file,data,512,&n);us=now_us()-t;
            zd_metric_record(mapped?&report->mapped_read:&report->ordinary_read,us,r,n,n!=512,&c[13]);
            if(r||n!=512||f_tell(file)!=position+512){if(!r)r=FR_INT_ERR;goto done;}
            for(unsigned i=0;i<512;++i)if(data[i]!=pattern(position+i)){r=FR_INT_ERR;goto done;}
            c[12]+=512;
        }
        c[6]=pair+1;
    }
done:
    seek_benchmark_count_fat=0;
    if(file_open){file->cltbl=NULL;FRESULT close=f_close(file);if(!r)r=close;}
    if(spacing_open){FRESULT close=f_close(spacing);if(!r)r=close;}
    if(spacing_created){FRESULT unlink=f_unlink(SPACING);if(!r)r=unlink;}
    c[1]=r;
    atomic_store_explicit(&report->complete,0x31424d53u,memory_order_release);
    // Allocation/media errors require the caller's conservative remount path.
    return r==FR_DISK_ERR||r==FR_NOT_READY?r:FR_OK;
}
#endif
