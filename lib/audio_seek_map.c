// Copyright 2026 Zack Scholl, GPLv3.0
#include "audio_seek_map.h"
#include "ff_clmt_validate.h"
#include "ff_workspace.h"
#include "seek_hash.h"
#include "diskio.h"
#include "seek_fragment_benchmark.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef SEEK_MAP_HOST_TEST
#include "pico/time.h"
#define now_us() time_us_32()
#else
static uint32_t now_us(void) { return 0; }
#endif

// Two headers: the checkpoint header is never overwritten during a transaction.
// A torn completion header therefore leaves all synchronized records resumable.
#define SCHEMA 1u
#define POLICY 1u
#define HEADER_MAGIC 0x31494d53u
#define RECORD_MAGIC 0x31524d53u
#define REF 0x8000u
#define VALID 0x0400u
#define LARGE 0x0800u
#define SOURCE 0x0200u
#define SLOT 0x01ffu
#define STATUS_MAP 1u
#define STATUS_LARGE 2u
typedef struct { uint16_t id, location; } directory_entry;
typedef struct {
    DWORD table[SEEK_MAP_WORDS];
    FSIZE_t size;
    DWORD cluster;
    uint32_t mount;
    uint16_t id;
    uint8_t pins;
    _Atomic uint8_t valid;
} cache_entry;
typedef struct {
    uint64_t generation;
    uint16_t slots, records;
    uint8_t bitmap[SEEK_MAP_RECORDS/8];
    bool valid, complete;
} generation;
typedef struct {
    FIL index;
    union { FIL file; struct { DIR dir; FILINFO info; } scan; } work;
    union { uint8_t bytes[512]; DWORD align; } record;
    DWORD table[SEEK_MAP_WORDS];
    cache_entry cache[SEEK_MAP_CACHE_ENTRIES];
    directory_entry directory[SEEK_MAP_FILES];
    generation generation[2];
    uint8_t identity[32];
    FATFS *fs;
    uint16_t count, pending_id, initial_id;
    bool have_initial, persistence_failed, sorted, manifest_complete, preparing, media_uncertain;
    int8_t writer;
    int8_t index_source;
    bool index_writable;
} seek_map_state;
// Allocate this fixed-size arena once, during media preparation after DSP
// initialization. Reserving it in .bss changes the existing reverb allocator's
// choice of comb count. It is never allocated/freed by the audio callback.
static seek_map_state *sm_storage;
#define sm (*sm_storage)
seek_map_stats seek_maps_stats;
static uint8_t *record(void) { return sm.record.bytes; }
static uint16_t u16(const uint8_t *p) { return p[0] | (uint16_t)p[1]<<8; }
static uint32_t u32(const uint8_t *p) { return u16(p) | (uint32_t)u16(p+2)<<16; }
static uint64_t u64(const uint8_t *p) { return u32(p) | (uint64_t)u32(p+4)<<32; }
static void p16(uint8_t *p,uint16_t v) { p[0]=v; p[1]=v>>8; }
static void p32(uint8_t *p,uint32_t v) { p16(p,v); p16(p+2,v>>16); }
static void p64(uint8_t *p,uint64_t v) { p32(p,v); p32(p+4,v>>32); }
static void error(FRESULT r) {
    seek_maps_stats.failures=seek_maps_stats.failures+1; seek_maps_stats.last_error=r;
    if(r==FR_DISK_ERR||r==FR_NOT_READY)sm.media_uncertain=true;
}
static void persistence_error(FRESULT r) { error(r); sm.persistence_failed=true; }
static void index_path(unsigned source,char path[32]) {
    snprintf(path,32,SEEK_MAP_DIRECTORY "/maps%u.bin",source);
}
void seek_maps_path(uint16_t id,char path[32]) {
    snprintf(path,32,"bank%u/%u.%u.wav",(id>>12)+1,(id>>8)&15,id&255);
}
static bool number(const char **p,unsigned maximum,unsigned *value) {
    const char *s=*p; unsigned n=0;
    if (*s<'0'||*s>'9') return false;
    if (*s=='0' && s[1]>='0' && s[1]<='9') return false;
    do { n=n*10+(*s++-'0'); if(n>maximum)return false; } while(*s>='0'&&*s<='9');
    *p=s; *value=n; return true;
}
bool seek_maps_file_id(const char *path,uint16_t *id) {
    unsigned bank,sample,variant;
    if (!path || strncmp(path,"bank",4)) return false;
    path+=4;
    if (!number(&path,16,&bank)||!bank||*path++!='/') return false;
    if (!number(&path,15,&sample)||*path++!='.') return false;
    if (!number(&path,255,&variant)||strcmp(path,".wav")) return false;
    *id=((bank-1)<<12)|(sample<<8)|variant; return true;
}
static int find(uint16_t id) {
    if (!sm.sorted) {
        for (unsigned i=0;i<sm.count;++i) if(sm.directory[i].id==id)return i;
        return -1;
    }
    unsigned lo=0,hi=sm.count;
    while(lo<hi) { unsigned mid=lo+(hi-lo)/2;
        if(sm.directory[mid].id<id)lo=mid+1;else hi=mid;
    }
    return lo<sm.count&&sm.directory[lo].id==id?(int)lo:-1;
}
static int add(uint16_t id) {
    int i=find(id); if(i>=0)return i;
    if(sm.count==SEEK_MAP_FILES) { seek_maps_stats.capacity=seek_maps_stats.capacity+1; return -1; }
    sm.sorted=false; i=sm.count++; sm.directory[i]=(directory_entry){id,0}; return i;
}
static void sort(void) {
    for(unsigned i=1;i<sm.count;++i) {
        directory_entry e=sm.directory[i]; unsigned j=i;
        while(j&&sm.directory[j-1].id>e.id) {sm.directory[j]=sm.directory[j-1];--j;}
        sm.directory[j]=e;
    }
    sm.sorted=true;
}
static FRESULT read_sector(FIL *file,unsigned sector) {
    UINT bytes=0; FRESULT r=f_lseek(file,(FSIZE_t)sector*512);
    if(!r)r=f_read(file,record(),512,&bytes);
    return r?r:bytes==512?FR_OK:FR_INT_ERR;
}
static FRESULT write_sector(FIL *file,unsigned sector) {
    UINT bytes=0; FRESULT r=f_lseek(file,(FSIZE_t)sector*512);
    if(!r)r=f_write(file,record(),512,&bytes);
    bool short_write=!r&&bytes!=512;
    if(!r)r=f_sync(file); // also flush a short allocation before normal fallback
    if(!r&&short_write)r=FR_DENIED;
    if(!r)seek_maps_stats.writes=seek_maps_stats.writes+1;
    return r;
}
static FRESULT close_index(void) {
    if(sm.index_source<0)return FR_OK;
    FRESULT r=f_close(&sm.index);sm.index_source=-1;sm.index_writable=false;
    if(r)error(r);return r;
}
static FRESULT open_index(unsigned source,bool write,bool truncate) {
    if(sm.index_source==(int)source&&(!write||sm.index_writable)&&!truncate)return FR_OK;
    FRESULT r=close_index();if(r)return r;
    char path[32];index_path(source,path);
    r=f_open(&sm.index,path,FA_READ|(write?FA_WRITE:0)|(truncate?FA_CREATE_ALWAYS:0));
    if(!r){sm.index_source=source;sm.index_writable=write;}
    return r;
}
static FRESULT read_index(unsigned source,unsigned sector) {
    uint32_t start=now_us();
    FRESULT r=open_index(source,false,false);if(!r)r=read_sector(&sm.index,sector);
    // Includes startup header/record reads and runtime cache record reads.
    // Map decoding/validation/building have separate accounting.
    seek_maps_stats.load_us=seek_maps_stats.load_us+(now_us()-start);return r;
}
static FRESULT write_index(unsigned source,unsigned sector) {
    FRESULT r=open_index(source,true,false);return r?r:write_sector(&sm.index,sector);
}
static bool checksum(void) { return u32(record()+508)==seek_crc32(record(),508); }
static void seal(void) { p32(record()+508,seek_crc32(record(),508)); }
static bool header_valid(bool complete,uint64_t generation) {
    uint8_t *r=record();
    return checksum() && u32(r)==HEADER_MAGIC && u32(r+4)==SCHEMA &&
        u32(r+8)==POLICY && u32(r+12)==complete && u64(r+16)!=0 &&
        (!generation || u64(r+16)==generation) && !memcmp(r+24,sm.identity,32) &&
        u32(r+56)<=SEEK_MAP_RECORDS && u32(r+60)<=SEEK_MAP_FILES;
}
static bool record_valid(unsigned source) {
    uint8_t *r=record(); char path[32];
    if(!checksum() || u32(r)!=RECORD_MAGIC || u64(r+4)!=sm.generation[source].generation ||
       u32(r+104)!=POLICY)return false;
    seek_maps_path(u16(r+12),path);
    if(memcmp(r+72,path,strlen(path)+1))return false;
    uint32_t words=u32(r+36);
    if(r[14]==STATUS_LARGE)return words>SEEK_MAP_WORDS && !(words&1);
    if(r[14]!=STATUS_MAP || words<2 || words>SEEK_MAP_WORDS || (words&1))return false;
    if(u32(r+128)!=words || u32(r+128+(words-1)*4))return false;
    uint64_t clusters=0;
    for(unsigned i=1;i<words-1;i+=2) {
        DWORD length=u32(r+128+i*4),start=u32(r+128+(i+1)*4);
        if(!length||start<2||start>=sm.fs->n_fatent||length>sm.fs->n_fatent-start)return false;
        clusters+=length;
    }
    uint64_t bytes=(uint64_t)sm.fs->csize*512, size=u64(r+16);
    return clusters==size/bytes+(size%bytes!=0) && clusters<=sm.fs->n_fatent-2;
}
static bool load_record(uint16_t location) {
    unsigned source=(location&SOURCE)!=0;
    if(!(location&REF)||!sm.generation[source].valid)return false;
    FRESULT r=read_index(source,2+(location&SLOT));
    if(r) { error(r); return false; }
    return record_valid(source);
}
static void table_decode(void) {
    memset(sm.table,0,sizeof sm.table);
    if(record()[14]==STATUS_MAP)
        for(unsigned i=0;i<u32(record()+36);++i)sm.table[i]=u32(record()+128+4*i);
}
static void retained(void) {
    unsigned n=0;for(unsigned i=0;i<SEEK_MAP_CACHE_ENTRIES;++i)
        if(sm.cache[i].valid)n+=sm.cache[i].table[0]*4;
    seek_maps_stats.retained_bytes=n;
}
static cache_entry *cache_store(uint16_t id,FSIZE_t size,DWORD cluster) {
    cache_entry *dest=NULL;
    for(unsigned i=0;i<SEEK_MAP_CACHE_ENTRIES;++i) {
        cache_entry *e=&sm.cache[i];
        if(e->pins)continue;
        if(e->valid&&e->id==id) {dest=e;break;}
        if(!e->valid)dest=e;
    }
    if(!dest)for(unsigned i=0;i<SEEK_MAP_CACHE_ENTRIES;++i) {
        cache_entry *e=&sm.cache[i];
        if(!e->pins && (!sm.preparing||!sm.have_initial||e->id!=sm.initial_id)) {dest=e;break;}
    }
    if(!dest)return NULL;
    if(dest->valid&&dest->id!=id)seek_maps_stats.evictions=seek_maps_stats.evictions+1;
    memcpy(dest->table,sm.table,sizeof sm.table);dest->id=id;dest->size=size;
    dest->cluster=cluster;dest->mount=seek_maps_stats.mount;dest->valid=true;
    retained();return dest;
}
bool seek_maps_unmount(void) {
    if(!sm_storage)return true;
    for(unsigned i=0;i<SEEK_MAP_CACHE_ENTRIES;++i)if(sm.cache[i].pins)return false;
    close_index(); memset(&sm,0,sizeof sm);sm.writer=sm.index_source=-1;
    seek_maps_stats.retained_bytes=seek_maps_stats.pending=0;return true;
}
bool seek_maps_media_ready(void) { return sm_storage&&sm.fs&&!sm.media_uncertain; }
static FRESULT identity(FATFS *fs,const uint8_t cid[16],uint64_t sectors) {
    if(disk_read(fs->pdrv,record(),fs->volbase,1)!=RES_OK)return FR_DISK_ERR;
    unsigned serial_offset=fs->fs_type==FS_EXFAT?100:fs->fs_type==FS_FAT32?67:39;
    uint32_t serial=u32(record()+serial_offset);
    memset(record(),0,128);memcpy(record(),cid,16);p64(record()+16,sectors);
    p64(record()+24,fs->volbase);p64(record()+32,fs->fatbase);p64(record()+40,fs->database);
    p64(record()+48,fs->dirbase);p64(record()+56,fs->bitbase);
    p32(record()+64,fs->n_fatent);p32(record()+68,fs->fsize);p32(record()+72,fs->csize);
    p32(record()+76,fs->fs_type);p32(record()+80,fs->n_fats);p32(record()+84,serial);
    seek_sha256_t hash;seek_sha256_init(&hash);seek_sha256_update(&hash,record(),88);
    seek_sha256_final(&hash,sm.identity);return FR_OK;
}
static void enumerate_bank(unsigned bank) {
    char path[32];snprintf(path,sizeof path,"bank%u",bank);
    FRESULT r=f_opendir(&sm.work.scan.dir,path);
    if(r==FR_NO_PATH||r==FR_NO_FILE)return;
    if(r) {error(r);sm.manifest_complete=false;return;}
    while(!(r=f_readdir(&sm.work.scan.dir,&sm.work.scan.info)) && sm.work.scan.info.fname[0]) {
        if(sm.work.scan.info.fattrib&AM_DIR)continue;
        // Normalize only ASCII case; numeric aliases with leading zeros are not
        // filenames that the player's canonical formatter can select.
        char *name=sm.work.scan.info.fname;unsigned length=strlen(name);
        if(length>14)continue;
        for(unsigned i=0;i<length;++i)if(name[i]>='A'&&name[i]<='Z')name[i]+='a'-'A';
        snprintf(path,sizeof path,"bank%u/%.14s",bank,name);
        uint16_t id;if(seek_maps_file_id(path,&id))add(id);
    }
    if(r) {error(r);sm.manifest_complete=false;}
    r=f_closedir(&sm.work.scan.dir);if(r){error(r);sm.manifest_complete=false;}
}
static void open_generation(unsigned source) {
    generation *g=&sm.generation[source];
    FRESULT r=open_index(source,false,false);
    if(r==FR_NO_FILE||r==FR_NO_PATH)return;
    if(r){error(r);return;}
    r=read_index(source,0);
    if(r){if(r==FR_DISK_ERR||r==FR_NOT_READY)error(r);return;}
    if(!header_valid(false,0))return;
    g->valid=true;g->generation=u64(record()+16);
    r=read_index(source,1);
    if(r==FR_DISK_ERR||r==FR_NOT_READY){error(r);return;}
    if(!r&&header_valid(true,g->generation)) {
        g->complete=true;g->slots=u32(record()+56);g->records=u32(record()+60);
        memcpy(g->bitmap,record()+64,sizeof g->bitmap);
        unsigned count=0;for(unsigned i=0;i<SEEK_MAP_RECORDS;++i)
            if(g->bitmap[i/8]&(1u<<(i%8))) {if(i>=g->slots){g->complete=false;break;}++count;}
        if(count!=g->records)g->complete=false;
    }
    if(!g->complete) {
        g->slots=0;g->records=0;memset(g->bitmap,0,sizeof g->bitmap);
        FSIZE_t bytes=f_size(&sm.index);
        FSIZE_t slots=bytes<=1024?0:(bytes-1024+511)/512;
        g->slots=slots>SEEK_MAP_RECORDS?SEEK_MAP_RECORDS:slots;
    }
}
static void merge_generation(unsigned source) {
    generation *g=&sm.generation[source];if(!g->valid)return;
    for(unsigned slot=0;slot<g->slots;++slot) {
        if(g->complete && !(g->bitmap[slot/8]&(1u<<(slot%8))))continue;
        FRESULT r=read_index(source,slot+2);
        if(r==FR_DISK_ERR||r==FR_NOT_READY){error(r);return;}
        if(r||!record_valid(source)) {seek_maps_stats.invalid=seek_maps_stats.invalid+1;continue;}
        int i=find(u16(record()+12));
        if(i>=0)sm.directory[i].location=REF|(source?SOURCE:0)|slot;
    }
}
static void make_header(unsigned source,bool complete) {
    generation *g=&sm.generation[source];memset(record(),0,512);
    p32(record(),HEADER_MAGIC);p32(record()+4,SCHEMA);p32(record()+8,POLICY);
    p32(record()+12,complete);p64(record()+16,g->generation);memcpy(record()+24,sm.identity,32);
    if(complete) {p32(record()+56,g->slots);p32(record()+60,g->records);memcpy(record()+64,g->bitmap,sizeof g->bitmap);}
    seal();
}
static bool start_writer(void) {
    if(sm.persistence_failed)return false;
    if(sm.writer>=0)return true;
    int newest=-1;
    for(unsigned i=0;i<2;++i)if(sm.generation[i].valid&&
        (newest<0||sm.generation[i].generation>sm.generation[newest].generation))newest=i;
    bool resume=newest>=0&&!sm.generation[newest].complete;
    unsigned source=resume?newest:newest<0?0:1-newest;
    generation *g=&sm.generation[source];
    FRESULT r=f_mkdir(SEEK_MAP_DIRECTORY);
    if(r!=FR_OK&&r!=FR_EXIST){persistence_error(r);return false;}
    r=open_index(source,true,!resume);
    if(r){persistence_error(r);return false;}
    if(!resume) {
        uint64_t next=newest<0?1:sm.generation[newest].generation+1;
        if(!next){persistence_error(FR_INT_ERR);return false;}
        memset(g,0,sizeof *g);g->valid=true;g->generation=next;
        make_header(source,false);r=write_index(source,0);
        if(!r){memset(record(),0,512);r=write_index(source,1);}
        if(r){persistence_error(r);return false;}
    }
    sm.writer=source;return true;
}
static bool save_record(unsigned entry) {
    if(sm.writer<0||sm.persistence_failed)return false;
    generation *g=&sm.generation[sm.writer];
    if(g->slots==SEEK_MAP_RECORDS){seek_maps_stats.capacity=seek_maps_stats.capacity+1;persistence_error(FR_DENIED);return false;}
    p64(record()+4,g->generation);seal();uint32_t start=now_us();
    FRESULT r=write_index(sm.writer,g->slots+2);
    seek_maps_stats.commit_us=seek_maps_stats.commit_us+(now_us()-start);
    if(r){persistence_error(r);return false;}
    sm.directory[entry].location=REF|VALID|(record()[14]==STATUS_LARGE?LARGE:0)|
        (sm.writer?SOURCE:0)|g->slots++;
    return true;
}
static void construct_record(uint16_t id,FIL *file,const ff_clmt_info *info,
                             uint32_t modified,uint8_t attr,bool large) {
    memset(record(),0,512);p32(record(),RECORD_MAGIC);p16(record()+12,id);
    record()[14]=large?STATUS_LARGE:STATUS_MAP;p64(record()+16,f_size(file));
    p32(record()+24,file->obj.sclust);p32(record()+28,modified);record()[32]=attr;
    p32(record()+36,large?info->fragments*2+2:sm.table[0]);memcpy(record()+40,info->digest,32);
    seek_maps_path(id,(char *)record()+72);p32(record()+104,POLICY);
    if(!large)for(unsigned i=0;i<sm.table[0];++i)p32(record()+128+i*4,sm.table[i]);
}
static void prepare_file(unsigned entry) {
    directory_entry *d=&sm.directory[entry];char path[32];seek_maps_path(d->id,path);
    FRESULT r=f_stat(path,&sm.work.scan.info);
    if(r){error(r);d->location=0;return;}
    uint32_t modified=(uint32_t)sm.work.scan.info.fdate<<16|sm.work.scan.info.ftime;
    uint8_t attr=sm.work.scan.info.fattrib;
    r=f_open(&sm.work.file,path,FA_READ);
    if(r){error(r);d->location=0;return;}
    bool loaded=load_record(d->location), reusable=false, unchanged=false,large=false;
    ff_clmt_info info;uint32_t start=now_us();
    if(loaded) {
        table_decode();large=record()[14]==STATUS_LARGE;
        r=ff_clmt_inspect(&sm.work.file,large?NULL:sm.table,large?0:sm.table[0],&info);
        reusable=!r && (!large||(!memcmp(info.digest,record()+40,32)&&info.fragments*2+2==u32(record()+36)));
        if(reusable)unchanged=u64(record()+16)==f_size(&sm.work.file)&&
            u32(record()+24)==sm.work.file.obj.sclust&&u32(record()+28)==modified&&record()[32]==attr;
        else seek_maps_stats.invalid=seek_maps_stats.invalid+1;
    }
    if(!reusable)r=ff_clmt_inspect(&sm.work.file,NULL,0,&info);
    seek_maps_stats.validation_us=seek_maps_stats.validation_us+(now_us()-start);
    if(r) {error(r);d->location=0;goto done;}
    seek_maps_stats.validated=seek_maps_stats.validated+1;
    if(reusable) {seek_maps_stats.reused=seek_maps_stats.reused+1;d->location|=VALID;}
    else {
        d->location=0;sm.table[0]=SEEK_MAP_WORDS;sm.work.file.cltbl=sm.table;
        start=now_us();seek_maps_stats.builds=seek_maps_stats.builds+1;
        r=f_lseek(&sm.work.file,CREATE_LINKMAP);
        seek_maps_stats.build_us=seek_maps_stats.build_us+(now_us()-start);sm.work.file.cltbl=NULL;
        large=r==FR_NOT_ENOUGH_CORE;
        if(r && !large){error(r);goto done;}
        if(sm.table[0]!=info.fragments*2+2){error(FR_INT_ERR);goto done;}
    }
    if(large)seek_maps_stats.oversized=seek_maps_stats.oversized+1;
    else cache_store(d->id,f_size(&sm.work.file),sm.work.file.obj.sclust);
    d->location|=VALID|(large?LARGE:0);
    if(!unchanged && start_writer()) {
        // start_writer uses the sector buffer. Keep the validated CLMT in its
        // separate, fixed workspace and construct the record only afterwards.
        construct_record(d->id,&sm.work.file,&info,modified,attr,large);save_record(entry);
    }
done:
    sm.work.file.cltbl=NULL;r=f_close(&sm.work.file);if(r)error(r);
}
static void finish(void) {
    if(!sm.manifest_complete||sm.media_uncertain)return;
    bool dirty=sm.writer>=0;
    unsigned newest=sm.generation[1].valid&&(!sm.generation[0].valid||
        sm.generation[1].generation>sm.generation[0].generation)?1:0;
    generation *latest=&sm.generation[newest];
    if(!latest->valid||!latest->complete||latest->records!=sm.count)dirty=true;
    for(unsigned i=0;i<sm.count;++i)if(!(sm.directory[i].location&VALID))dirty=true;
    if(!dirty)return;
    if(!start_writer())return;
    for(unsigned i=0;i<sm.count;++i) {
        uint16_t location=sm.directory[i].location;
        if(!(location&VALID))return; // no completion marker for uncertain media
        if(((location&SOURCE)!=0)==sm.writer)continue;
        if(!load_record(location)){persistence_error(FR_INT_ERR);return;}
        if(!save_record(i))return;
    }
    generation *g=&sm.generation[sm.writer];memset(g->bitmap,0,sizeof g->bitmap);
    for(unsigned i=0;i<sm.count;++i) {
        unsigned slot=sm.directory[i].location&SLOT;g->bitmap[slot/8]|=1u<<(slot%8);
    }
    g->records=sm.count;make_header(sm.writer,true);uint32_t start=now_us();
    FRESULT r=write_index(sm.writer,1);seek_maps_stats.commit_us=seek_maps_stats.commit_us+(now_us()-start);
    if(r){persistence_error(r);return;}
    g->complete=true;seek_maps_stats.commits=seek_maps_stats.commits+1;seek_maps_stats.generation=g->generation;
    sm.writer=-1;
}
FRESULT seek_maps_prepare(FATFS *fs,const uint8_t cid[16],uint64_t sectors,const char *initial) {
    if(!fs||!cid||!fs->fs_type||!fs->csize||fs->n_fatent<2)return FR_INVALID_PARAMETER;
    if(!sm_storage) {
        sm_storage=calloc(1,sizeof *sm_storage);
        if(!sm_storage) {
            seek_maps_stats.failures=seek_maps_stats.failures+1;
            seek_maps_stats.last_error=FR_NOT_ENOUGH_CORE;
            return FR_NOT_ENOUGH_CORE;
        }
        sm.writer=sm.index_source=-1;
    }
    if(!seek_maps_unmount())return FR_LOCKED;
    uint32_t mount=seek_maps_stats.mount+1;memset(&seek_maps_stats,0,sizeof seek_maps_stats);
    seek_maps_stats.mount=mount?mount:1;
    seek_maps_stats.reserved_bytes=sizeof sm+sizeof seek_maps_stats+sizeof sm_storage+ff_workspace_reserved_bytes();
#if SEEK_TEST_FRAGMENT_BENCH
    FRESULT bench=seek_fragment_benchmark(fs,&sm.work.file,&sm.index,sm.table,record());
    if(bench){error(bench);return bench;}
#endif
    uint32_t start=now_us();sm.fs=fs;sm.manifest_complete=sm.preparing=true;
    FRESULT r=identity(fs,cid,sectors);if(r){error(r);sm.fs=NULL;return r;}
    sm.have_initial=seek_maps_file_id(initial,&sm.initial_id);
    if(sm.have_initial) {
        char path[32];seek_maps_path(sm.initial_id,path);
        if(!f_stat(path,&sm.work.scan.info))add(sm.initial_id);
        enumerate_bank((sm.initial_id>>12)+1);
    }
    for(unsigned bank=1;bank<=16;++bank)
        if(!sm.have_initial||bank!=(unsigned)(sm.initial_id>>12)+1)enumerate_bank(bank);
    open_generation(0);open_generation(1);
    unsigned newest=sm.generation[1].valid&&(!sm.generation[0].valid||
        sm.generation[1].generation>sm.generation[0].generation)?1:0;
    if(!sm.generation[newest].complete&&sm.generation[1-newest].complete)
        merge_generation(1-newest);
    merge_generation(newest);
    for(unsigned i=0;i<sm.count&&!sm.media_uncertain;++i)prepare_file(i);
    finish();sort();seek_maps_stats.files=sm.count;
    for(unsigned i=0;i<2;++i)if(sm.generation[i].valid&&
        sm.generation[i].generation>seek_maps_stats.generation)seek_maps_stats.generation=sm.generation[i].generation;
    seek_maps_stats.prepare_us=now_us()-start;sm.preparing=false;
    if(sm.media_uncertain) {
        for(unsigned i=0;i<sm.count;++i)sm.directory[i].location&=~VALID;
        memset(sm.cache,0,sizeof sm.cache);retained();return FR_DISK_ERR;
    }
    return FR_OK;
}
void seek_maps_detach(FIL *file) {
    if(!file)return;
    if(!sm_storage){file->cltbl=NULL;return;}
    for(unsigned i=0;i<SEEK_MAP_CACHE_ENTRIES;++i)
        if(file->cltbl==sm.cache[i].table && sm.cache[i].pins)--sm.cache[i].pins;
    file->cltbl=NULL;
}
seek_map_job seek_maps_request(const char *path) {
    seek_maps_stats.pending=SEEK_MAP_IDLE;
    uint16_t id;if(!sm_storage||!sm.fs||sm.media_uncertain||!seek_maps_file_id(path,&id))return SEEK_MAP_IDLE;
    int i=find(id);sm.pending_id=id;
    if(i<0&&sm.count==SEEK_MAP_FILES)return SEEK_MAP_IDLE;
    // Known oversized or RAM-only outcomes do not cause repeated builds during
    // this mount. Without a persisted record, eviction means ordinary seeking.
    if(i>=0 && (sm.directory[i].location&VALID) &&
       (!(sm.directory[i].location&REF)||(sm.directory[i].location&LARGE)))return SEEK_MAP_IDLE;
    seek_maps_stats.pending=i>=0&&(sm.directory[i].location&VALID)?SEEK_MAP_LOAD:SEEK_MAP_PREPARE;
    return seek_maps_stats.pending;
}
bool seek_maps_attach(FIL *file,const char *path) {
    seek_maps_detach(file);uint16_t id;
    if(!sm_storage||!sm.fs||sm.media_uncertain||file->obj.fs!=sm.fs||file->obj.id!=sm.fs->id||!seek_maps_file_id(path,&id))return false;
    for(unsigned i=0;i<SEEK_MAP_CACHE_ENTRIES;++i) {
        cache_entry *e=&sm.cache[i];
        if(!e->valid||e->id!=id||e->mount!=seek_maps_stats.mount)continue;
        if(e->size!=f_size(file)||e->cluster!=file->obj.sclust) {seek_maps_invalidate(path);break;}
        if(e->pins==255)break;
        seek_maps_stats.hits=seek_maps_stats.hits+1;
#if defined(SEEK_MAP_ATTACH) && !SEEK_MAP_ATTACH
        // Comparison builds retain the same lookup/load/cache workload and
        // suppress only attachment. A loaded hit must not queue another load.
        return false;
#else
        ++e->pins;file->cltbl=e->table;return true;
#endif
    }
    seek_maps_stats.misses=seek_maps_stats.misses+1;seek_maps_request(path);return false;
}
bool seek_maps_invalidate(const char *path) {
    if(!sm_storage)return true;
    uint16_t id;if(!seek_maps_file_id(path,&id))return true;
    for(unsigned n=0;n<SEEK_MAP_CACHE_ENTRIES;++n)
        if(sm.cache[n].id==id&&sm.cache[n].pins)return false;
    int i=find(id);if(i>=0)sm.directory[i].location&=~VALID;
    for(unsigned n=0;n<SEEK_MAP_CACHE_ENTRIES;++n)if(sm.cache[n].id==id)sm.cache[n].valid=false;
    seek_maps_stats.invalid=seek_maps_stats.invalid+1;retained();seek_maps_request(path);return true;
}
void seek_maps_service(void) {
    if(!sm_storage)return;
    unsigned job=seek_maps_stats.pending;uint16_t id=sm.pending_id;seek_maps_stats.pending=SEEK_MAP_IDLE;
    if(!job||!sm.fs)return;
    int i=find(id);
    if(job==SEEK_MAP_LOAD && i>=0 && (sm.directory[i].location&VALID)) {
        if(load_record(sm.directory[i].location)) {
            if(record()[14]==STATUS_MAP) {
                table_decode();
                if(cache_store(id,u64(record()+16),u32(record()+24)))seek_maps_stats.loads=seek_maps_stats.loads+1;
            }
        } else {sm.directory[i].location&=~VALID;error(FR_INT_ERR);}
    } else {
        if(i<0)i=add(id);
        if(i>=0){prepare_file(i);finish();sort();seek_maps_stats.files=sm.count;}
    }
}
#ifndef SEEK_MAP_HOST_TEST
__attribute__((section(".time_critical.seek_layout")))
#endif
void seek_maps_layout(uint16_t id,volatile uint32_t out[76]) {
    for(unsigned i=0;i<76;i+=4)out[i]=out[i+1]=out[i+2]=out[i+3]=0;
    out[0]=0x314d4c43u;out[2]=id;out[3]=seek_maps_stats.mount;
    if(!seek_maps_media_ready())return;
    for(unsigned i=0;i<SEEK_MAP_CACHE_ENTRIES;++i) {
        const cache_entry *e=&sm.cache[i];
        if(!atomic_load_explicit(&e->valid,memory_order_acquire)||e->id!=id||
           e->mount!=seek_maps_stats.mount)continue;
        out[4]=e->size;out[5]=(uint64_t)e->size>>32;out[6]=e->cluster;
        out[7]=e->table[0];out[8]=sm.fs->csize;out[9]=sm.fs->n_fatent;out[10]=POLICY;
        for(unsigned word=0;word<SEEK_MAP_WORDS;word+=4) {
            out[12+word]=e->table[word];out[13+word]=e->table[word+1];
            out[14+word]=e->table[word+2];out[15+word]=e->table[word+3];
        }
        // Core 1 can invalidate a cache entry but cannot replace its table.
        // A concurrent invalidation makes this response unavailable.
        out[1]=atomic_load_explicit(&e->valid,memory_order_acquire)?1:0;
        return;
    }
}
