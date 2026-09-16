// Exercise the repository's existing FatFs API before the new cache/index.
#include "disk.h"
#include "ff_clmt_validate.h"
#include "ff_workspace.h"
#include "seek_hash.h"
#include "audio_seek_map.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "definitions.h"
#include "savefile.h"
static FIL fil_current;
static char fil_current_name[32];
static volatile bool fil_is_open;
static uint32_t last_seeked;
static bool fail_arena_allocation;
void *__real_calloc(size_t count,size_t bytes);
void *__wrap_calloc(size_t count,size_t bytes) {
    return fail_arena_allocation?NULL:__real_calloc(count,bytes);
}
#define zd_f_open f_open
#define zd_f_close f_close
#define SRAM_END 0
static void watchdog_reboot(unsigned pc,unsigned sp,unsigned delay) {
    (void)pc;(void)sp;(void)delay;assert(!"unexpected recovery request");
}
#include "audio_media.h"
static void checked(FRESULT r, int line) { if (r) fprintf(stderr, "FatFs error %u at line %d\n", r, line); assert(r == FR_OK); }
#define ok(call) checked((call), __LINE__)
static BYTE pattern(unsigned n) { return (BYTE)((n*31u) ^ (n>>9)); }
extern DWORD ff_test_clmt_order(FSIZE_t sectors,UINT cluster_sectors);
static void cluster_order_arithmetic(void) {
    uint64_t seed=0x912ac044deadbeefULL;
    const uint64_t boundaries[]={0,1,511,512,UINT32_MAX,0x100000000ULL,
        0x100000001ULL,0x7fffffffffffffffULL,UINT64_MAX};
    for(unsigned size=1;size<=32768;size<<=1) {
        for(unsigned i=0;i<sizeof boundaries/sizeof boundaries[0];++i)
            assert(ff_test_clmt_order(boundaries[i],size)==(DWORD)(boundaries[i]/size));
        for(unsigned i=0;i<10000;++i) {
            seed=seed*6364136223846793005ULL+1;
            assert(ff_test_clmt_order(seed,size)==(DWORD)(seed/size));
        }
    }
    puts("map cluster arithmetic: 160000 full-width offsets and boundaries match division for every supported cluster size");
}
static void hashes(void) {
    const char *expect[] = {
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
        "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"};
    for (unsigned v=0;v<3;++v) {
        seek_sha256_t hash; BYTE digest[32]; char hex[65];
        seek_sha256_init(&hash);
        if (v==1) { seek_sha256_update(&hash,"a",1); seek_sha256_update(&hash,"bc",2); }
        if (v==2) for (unsigned i=0;i<1000000;++i) seek_sha256_update(&hash,"a",1);
        seek_sha256_final(&hash,digest);
        for(unsigned i=0;i<32;++i) sprintf(hex+2*i,"%02x",digest[i]);
        assert(!strcmp(hex,expect[v]));
    }
    assert(seek_crc32("123456789",9)==0xcbf43926);
}
static void inspect(FIL *file, DWORD *table) {
    ff_clmt_info info, without;
    FIL before = *file;
    ok(ff_clmt_inspect(file,table,table[0],&info));
    assert(!memcmp(file,&before,sizeof before));
    ok(ff_clmt_inspect(file,NULL,0,&without));
    assert(!memcmp(&info,&without,sizeof info));
    assert(info.fragments==(table[0]-2)/2);
    DWORD saved=table[2]; table[2]++;
    assert(ff_clmt_inspect(file,table,table[0],&info)!=FR_OK); table[2]=saved;
    saved=table[1]; table[1]=0;
    assert(ff_clmt_inspect(file,table,table[0],&info)!=FR_OK); table[1]=saved;
    saved=table[table[0]-1]; table[table[0]-1]=2;
    assert(ff_clmt_inspect(file,table,table[0],&info)!=FR_OK); table[table[0]-1]=saved;
    file->obj.fs->winsect=(LBA_t)-1; test_read_fail_after=0;
    assert(ff_clmt_inspect(file,table,table[0],&info)==FR_DISK_ERR);
    test_read_fail_after=-1;
    assert(!memcmp(file,&before,sizeof before));
}
static void create_file(const char *path,unsigned length) {
    FIL file;UINT written;BYTE data[512];
    ok(f_open(&file,path,FA_CREATE_ALWAYS|FA_WRITE));
    for(unsigned offset=0;offset<length;) {
        unsigned n=length-offset;if(n>sizeof data)n=sizeof data;
        for(unsigned i=0;i<n;++i)data[i]=pattern(offset+i);
        ok(f_write(&file,data,n,&written));assert(written==n);offset+=n;
    }
    ok(f_close(&file));
}
static void corrupt_link(FIL *file,DWORD cluster) {
    FATFS *fs=file->obj.fs;
    unsigned width=fs->fs_type==FS_FAT12?2:fs->fs_type==FS_FAT16?2:4;
    uint64_t offset=fs->fatbase*512+(fs->fs_type==FS_FAT12?cluster+cluster/2:cluster*width);
    BYTE saved[4];memcpy(saved,test_disk+offset,width);
    DWORD next=cluster; // cycle, retaining all file metadata
    if(fs->fs_type==FS_FAT12) {
        unsigned old=test_disk[offset]|test_disk[offset+1]<<8;
        next=cluster&1?(old&15)|(next<<4):(old&0xf000)|next;
    }
    for(unsigned i=0;i<width;++i)test_disk[offset+i]=next>>(8*i);
    fs->winsect=(LBA_t)-1;ff_clmt_info info;
    assert(ff_clmt_inspect(file,NULL,0,&info)!=FR_OK);
    memcpy(test_disk+offset,saved,width);fs->winsect=(LBA_t)-1;
}
static void contiguous(FATFS *fs) {
    create_file("contiguous.bin",fs->csize*512*9+17);
    FIL file;ok(f_open(&file,"contiguous.bin",FA_READ));DWORD map[64]={64};
    file.cltbl=map;ok(f_lseek(&file,CREATE_LINKMAP));assert(map[0]==4);
    inspect(&file,map);
    // Cached-cluster reuse must preserve FatFs's preceding-byte convention at
    // exact cluster/sector boundaries, including after a crossing read.
    unsigned cluster=fs->csize*512, length=cluster*9+17;
    unsigned offsets[]={cluster+44,cluster+80,cluster,cluster-1,cluster+1,
        2*cluster,2*cluster-1,2*cluster+1,511,512,513,length-1,0};
    for(unsigned i=0;i<sizeof offsets/sizeof offsets[0];++i) {
        BYTE data[128];UINT got,want=length-offsets[i];if(want>sizeof data)want=sizeof data;
        ok(f_lseek(&file,offsets[i]));assert(f_tell(&file)==offsets[i]);
        ok(f_read(&file,data,want,&got));assert(got==want&&f_tell(&file)==offsets[i]+got);
        for(unsigned j=0;j<got;++j)assert(data[j]==pattern(offsets[i]+j));
    }
    if(fs->fs_type==FS_EXFAT) {
        assert(file.obj.stat==2);
        DWORD bit=file.obj.sclust+4-2;uint64_t offset=fs->bitbase*512+bit/8;
        BYTE saved=test_disk[offset];test_disk[offset]&=~(1u<<(bit%8));
        fs->winsect=(LBA_t)-1;ff_clmt_info info;
        assert(ff_clmt_inspect(&file,map,map[0],&info)!=FR_OK);
        test_disk[offset]=saved;fs->winsect=(LBA_t)-1;
    } else corrupt_link(&file,file.obj.sclust+4);
    file.cltbl=NULL;ok(f_close(&file));ok(f_unlink("contiguous.bin"));
}
static void reboot(FATFS *fs) {
    assert(seek_maps_unmount());
    ok(f_mount(NULL,"0:",0));memset(fs,0,sizeof *fs);ok(f_mount(fs,"0:",1));
}
static const uint8_t cid[16]={1,2,3,4,5,6,7,8};
static void prepare(FATFS *fs) {
    ok(seek_maps_prepare(fs,cid,test_sectors,"bank1/1.0.wav"));
}
static void cache_index(FATFS *fs) {
    for(unsigned i=1;i<=3;++i) {char path[32];snprintf(path,sizeof path,"bank1/%u.0.wav",i);create_file(path,fs->csize*512*9+17);}
    FIL live;ok(f_open(&live,"bank1/1.0.wav",FA_READ));ok(f_lseek(&live,123));
    FIL untouched=live;
    static bool tested_allocation_failure;
    if(!tested_allocation_failure) {
        fail_arena_allocation=true;
        assert(seek_maps_prepare(fs,cid,test_sectors,"bank1/1.0.wav")==FR_NOT_ENOUGH_CORE);
        ok(audio_file_open("bank1/1.0.wav"));assert(fil_is_open&&!fil_current.cltbl);
        ok(audio_file_close());fail_arena_allocation=false;tested_allocation_failure=true;
    }
    prepare(fs);
    assert(!memcmp(&live,&untouched,sizeof live));ok(f_close(&live));
    assert(seek_maps_stats.files==4 && seek_maps_stats.builds==4);
    assert(seek_maps_stats.oversized==1 && seek_maps_stats.commits==1 && !seek_maps_stats.failures);
    assert(seek_maps_request("bank1/0.0.wav")==SEEK_MAP_IDLE);
    FIL first,second;ok(f_open(&first,"bank1/1.0.wav",FA_READ));
    assert(seek_maps_attach(&first,"bank1/1.0.wav"));
    DWORD *pinned=first.cltbl;ok(f_lseek(&first,123));
    uint32_t layout[76];unsigned reads_before=test_reads,writes_before=test_writes;
    seek_maps_layout(0x100,layout);
    assert(layout[0]==0x314d4c43&&layout[1]&&layout[2]==0x100);
    assert(layout[4]==f_size(&first)&&layout[6]==first.obj.sclust&&layout[7]==pinned[0]);
    assert(!memcmp(layout+12,pinned,pinned[0]*4));
    assert(test_reads==reads_before&&test_writes==writes_before&&f_tell(&first)==123);
    seek_maps_layout(0xffff,layout);assert(!layout[1]);
    assert(!seek_maps_unmount());
    assert(!seek_maps_invalidate("bank1/1.0.wav"));assert(first.cltbl==pinned);
    for(unsigned pass=0;pass<8;++pass) {
        const char *path=pass%2?"bank1/2.0.wav":"bank1/3.0.wav";
        assert(seek_maps_request(path)==SEEK_MAP_LOAD);seek_maps_service();
        ok(f_open(&second,path,FA_READ));assert(seek_maps_attach(&second,path));
        assert(first.cltbl==pinned && f_tell(&first)==123 && !first.err);
        BYTE data[523];UINT n;ok(f_lseek(&second,511));ok(f_read(&second,data,sizeof data,&n));
        for(unsigned i=0;i<n;++i)assert(data[i]==pattern(511+i));
        seek_maps_detach(&second);ok(f_close(&second));
    }
    assert(seek_maps_stats.builds==4 && seek_maps_stats.loads==8);
    seek_maps_detach(&first);ok(f_close(&first));
    ok(audio_file_open("bank1/1.0.wav"));assert(fil_is_open&&fil_current.cltbl);
    pinned=fil_current.cltbl;ok(f_lseek(&fil_current,123));
    ok(audio_file_open(fil_current_name));
    assert(f_tell(&fil_current)==0&&fil_current.cltbl==pinned&&last_seeked==UINT32_MAX);
    assert(audio_file_open("bank1/15.255.wav")==FR_NO_FILE);
    assert(!fil_is_open&&!fil_current.cltbl&&!fil_current.obj.fs);
    ok(audio_file_open("bank1/2.0.wav"));assert(fil_is_open&&fil_current.cltbl);
    assert(fil_current.cltbl!=pinned);ok(audio_file_close());
    assert(!fil_is_open&&!fil_current.cltbl);
#if AUDIO_PREPARE_NEXT
    ok(audio_file_open("bank1/2.0.wav"));
    ok(f_lseek(&fil_current,123));FIL before_prepare=fil_current;
    unsigned before_writes=test_writes;
    audio_prepare_step("bank1/3.0.wav");
    audio_prepare_step("bank1/3.0.wav");
    assert(audio_prepare_ready("bank1/3.0.wav"));
    audio_prepare_warm(513);
    assert(!memcmp(&before_prepare,&fil_current,sizeof fil_current));
    assert(test_writes==before_writes);
    ok(audio_file_open("bank1/3.0.wav"));
    assert(f_tell(&fil_current)==513&&fil_current.cltbl&&audio_prepare_adopts);
    BYTE prefetched[700];UINT prefetched_count;
    ok(f_read(&fil_current,prefetched,sizeof prefetched,&prefetched_count));
    assert(prefetched_count==sizeof prefetched);
    for(unsigned k=0;k<sizeof prefetched;++k)assert(prefetched[k]==pattern(513+k));
    audio_prepare_step("bank1/1.0.wav");
    audio_prepare_step("bank1/2.0.wav"); // superseded request closes its handle
    ok(audio_file_close());assert(!audio_prepared_stage);
    audio_prepare_step("bank1/15.255.wav");
    assert(audio_prepare_ready("bank1/15.255.wav"));
    assert(audio_file_open("bank1/15.255.wav")==FR_NO_FILE);
    assert(!fil_is_open&&!audio_prepared_stage&&test_writes==before_writes);
#endif
    reboot(fs);
    unsigned writes=test_writes;prepare(fs);
    assert(seek_maps_stats.builds==0&&seek_maps_stats.reused==4&&seek_maps_stats.writes==0&&test_writes==writes);
    assert(!seek_maps_stats.failures);
    // Settings and metadata changes leave allocation maps reusable.
    create_file("savefile0",513);reboot(fs);writes=test_writes;prepare(fs);
    assert(!seek_maps_stats.builds&&!seek_maps_stats.writes&&test_writes==writes);
    assert(seek_maps_unmount());
    test_fattime+=65536;
    ok(f_open(&first,"bank1/1.0.wav",FA_WRITE));
    UINT written;BYTE same=pattern(0);ok(f_write(&first,&same,1,&written));ok(f_close(&first));
    reboot(fs);prepare(fs);
    assert(!seek_maps_stats.builds&&seek_maps_stats.reused==4&&seek_maps_stats.commits==1);
    assert(seek_maps_unmount());ok(f_unlink("bank1/2.0.wav"));reboot(fs);prepare(fs);
    assert(!seek_maps_stats.builds&&seek_maps_stats.files==3&&seek_maps_stats.commits==1);
    reboot(fs);writes=test_writes;prepare(fs);
    assert(!seek_maps_stats.builds&&!seek_maps_stats.writes&&test_writes==writes);
    // A read-only unchanged card still reuses its index with zero write attempts.
    reboot(fs);test_readonly=1;prepare(fs);
    assert(!seek_maps_stats.builds&&!seek_maps_stats.writes&&!seek_maps_stats.failures);
    test_readonly=0;assert(seek_maps_unmount());
    printf("cache/index: %u bytes reserved on host; restart, eviction, metadata, deletion and read-only reuse pass\n",seek_maps_stats.reserved_bytes);
}
static void persistence_faults(FATFS *fs) {
    for(unsigned i=4;i<=6;++i) {char path[32];snprintf(path,sizeof path,"bank1/%u.0.wav",i);create_file(path,fs->csize*512*9+17);}
    reboot(fs);
    size_t bytes=test_sectors*512;BYTE *before=malloc(bytes);assert(before);memcpy(before,test_disk,bytes);
    unsigned trials=0;
    for(int failure=0;failure<90;++failure) {
        assert(seek_maps_unmount());ok(f_mount(NULL,"0:",0));
        memcpy(test_disk,before,bytes);ok(f_mount(fs,"0:",1));
        test_write_fail_after=failure;
        FRESULT result=seek_maps_prepare(fs,cid,test_sectors,"bank1/1.0.wav");
        assert(result==FR_OK||result==FR_DISK_ERR);
        unsigned saved=seek_maps_stats.writes>2?seek_maps_stats.writes-2:0;if(saved>3)saved=3;
        // A failed disk write may leave FatFs's shared window dirty and make
        // later file validation unavailable until remount.
        assert(seek_maps_stats.builds<=3);
        // Keep the fault active while discarding RAM state so closing cannot
        // accidentally finish a write that did not reach storage in the trial.
        assert(seek_maps_unmount());ok(f_mount(NULL,"0:",0));test_write_fail_after=-1;
        ok(f_mount(fs,"0:",1));prepare(fs);
        if(seek_maps_stats.builds!=3-saved) {
            fprintf(stderr,"failure %d, saved %u, restart builds %u, errors %u/%u\n",failure,saved,seek_maps_stats.builds,seek_maps_stats.failures,seek_maps_stats.last_error);
        }
        assert(seek_maps_stats.builds==3-saved&&!seek_maps_stats.failures);
        reboot(fs);unsigned writes=test_writes;prepare(fs);
        assert(!seek_maps_stats.builds&&!seek_maps_stats.writes&&test_writes==writes);
        ++trials;
    }
    assert(seek_maps_unmount());free(before);
    printf("index recovery: %u injected write-failure points; every saved new map reused\n",trials);
}
static void put32(BYTE *p,DWORD value) {for(unsigned i=0;i<4;++i)p[i]=value>>(i*8);}
static void allocation_change(FATFS *fs) {
    // Relocate two interior clusters while preserving the path, start, length,
    // timestamps AND logical contents. A metadata fingerprint cannot detect it.
    assert(seek_maps_unmount());FIL file;
    ok(f_open(&file,"bank1/1.0.wav",FA_READ));DWORD map[64]={64};
    file.cltbl=map;ok(f_lseek(&file,CREATE_LINKMAP));assert(map[0]==4);
    DWORD a=file.obj.sclust+3,b=a+1;file.cltbl=NULL;ok(f_close(&file));
    put32(test_disk+fs->fatbase*512+(a-1)*4,b);
    put32(test_disk+fs->fatbase*512+b*4,a);
    put32(test_disk+fs->fatbase*512+a*4,b+1);
    size_t bytes=fs->csize*512;BYTE *temp=malloc(bytes);assert(temp);
    BYTE *ap=test_disk+(fs->database+(a-2)*fs->csize)*512;
    BYTE *bp=test_disk+(fs->database+(b-2)*fs->csize)*512;
    memcpy(temp,ap,bytes);memcpy(ap,bp,bytes);memcpy(bp,temp,bytes);free(temp);
    reboot(fs);prepare(fs);assert(seek_maps_stats.builds==1&&seek_maps_stats.invalid==1);
    reboot(fs);prepare(fs);assert(!seek_maps_stats.builds&&!seek_maps_stats.writes);
    ok(f_open(&file,"bank1/1.0.wav",FA_READ));assert(seek_maps_attach(&file,"bank1/1.0.wav"));
    BYTE data[512];UINT n;
    for(unsigned offset=0;offset<f_size(&file);) {
        ok(f_read(&file,data,sizeof data,&n));assert(n);
        for(unsigned i=0;i<n;++i)assert(data[i]==pattern(offset+i));offset+=n;
    }
    seek_maps_detach(&file);ok(f_close(&file));assert(seek_maps_unmount());
    puts("allocation validation: metadata-preserving interior relocation detected and rebuilt once");
}
static void damaged_commit(FATFS *fs) {
    prepare(fs);unsigned source=(seek_maps_stats.generation-1)&1;
    assert(seek_maps_unmount());char path[32];snprintf(path,sizeof path,SEEK_MAP_DIRECTORY "/maps%u.bin",source);
    FIL file;ok(f_open(&file,path,FA_WRITE));ok(f_lseek(&file,512+508));
    BYTE bad[4]={0};UINT n;ok(f_write(&file,bad,4,&n));ok(f_close(&file));
    reboot(fs);prepare(fs);
    assert(!seek_maps_stats.builds&&seek_maps_stats.commits==1&&!seek_maps_stats.failures);
    reboot(fs);prepare(fs);assert(!seek_maps_stats.builds&&!seek_maps_stats.writes);
    // A single corrupt record does not discard the other checkpoint records,
    // including those beyond the corrupt sector in this generation.
    source=(seek_maps_stats.generation-1)&1;assert(seek_maps_unmount());
    snprintf(path,sizeof path,SEEK_MAP_DIRECTORY "/maps%u.bin",source);
    ok(f_open(&file,path,FA_WRITE));ok(f_lseek(&file,1024+508));
    ok(f_write(&file,bad,4,&n));ok(f_close(&file));reboot(fs);prepare(fs);
    assert(seek_maps_stats.builds==1&&seek_maps_stats.commits==1);
    reboot(fs);prepare(fs);assert(!seek_maps_stats.builds&&!seek_maps_stats.writes);
    assert(seek_maps_unmount());puts("index corruption: torn completion marker and isolated record recovered");
}
static void bounded_workspace_and_full_media(void) {
    void *workspace=ff_memalloc(FF_NAME_WORKSPACE_BYTES);
    assert(workspace && !((uintptr_t)workspace%sizeof(DWORD)));
    assert(!ff_memalloc(64));ff_memfree(NULL);assert(!ff_memalloc(64));
    ff_memfree(workspace);assert(!ff_memalloc(32768));
    FATFS fs;test_disk_create(65536);BYTE work[4096];
    MKFS_PARM options={.fmt=FM_FAT|FM_SFD,.n_fat=1,.align=1,.au_size=65536};
    ok(f_mkfs("0:",&options,work,sizeof work));ok(f_mount(&fs,"0:",1));
    assert(fs.csize==128);ok(f_mkdir("bank1"));create_file("bank1/1.0.wav",12345);
    test_readonly=1;prepare(&fs);
    assert(seek_maps_stats.builds==1&&!seek_maps_stats.writes&&seek_maps_stats.failures==1);
    assert(seek_maps_media_ready()&&seek_maps_request("bank1/1.0.wav")==SEEK_MAP_IDLE);
    ok(audio_file_open("bank1/1.0.wav"));assert(fil_current.cltbl);ok(audio_file_close());
    assert(seek_maps_unmount());test_readonly=0;
    FIL filler;ok(f_open(&filler,"filler.bin",FA_WRITE|FA_CREATE_ALWAYS));
    memset(work,0,sizeof work);UINT written;
    do {ok(f_write(&filler,work,sizeof work,&written));} while(written==sizeof work);
    ok(f_close(&filler));reboot(&fs);prepare(&fs);
    assert(seek_maps_stats.builds==1&&!seek_maps_stats.writes&&seek_maps_stats.failures==1);
    assert(seek_maps_media_ready());
    ok(audio_file_open("bank1/1.0.wav"));assert(fil_current.cltbl);
    BYTE data[512];UINT n;ok(f_lseek(&fil_current,511));ok(f_read(&fil_current,data,sizeof data,&n));
    for(unsigned i=0;i<n;++i)assert(data[i]==pattern(511+i));ok(audio_file_close());
    assert(seek_maps_unmount());ok(f_unlink("filler.bin"));reboot(&fs);prepare(&fs);
    assert(seek_maps_stats.builds==1&&seek_maps_stats.commits==1&&!seek_maps_stats.failures);
    reboot(&fs);unsigned writes=test_writes;prepare(&fs);
    assert(!seek_maps_stats.builds&&!seek_maps_stats.writes&&test_writes==writes);
    assert(seek_maps_unmount());ok(f_mount(NULL,"0:",0));
    puts("bounded FatFs workspace: 64 KiB clusters, read-only/full-card playback, and writable recovery pass");
}
static void manifest_capacity(void) {
    FATFS fs;test_disk_create(65536);BYTE work[4096];
    MKFS_PARM options={.fmt=FM_FAT|FM_SFD,.n_fat=1,.align=1,.au_size=512};
    ok(f_mkfs("0:",&options,work,sizeof work));ok(f_mount(&fs,"0:",1));
    ok(f_mkdir("bank1"));ok(f_mkdir("bank16"));
    for(unsigned i=0;i<256;++i) {
        char path[32];snprintf(path,sizeof path,"bank1/0.%u.wav",i);create_file(path,7+i%3);
    }
    create_file("bank16/15.255.wav",2);
    const char *initial="bank16/15.255.wav";
    ok(seek_maps_prepare(&fs,cid,test_sectors,initial));
    assert(seek_maps_stats.files==256&&seek_maps_stats.builds==256&&seek_maps_stats.capacity==1);
    assert(!seek_maps_stats.failures&&seek_maps_stats.commits==1);
    ok(audio_file_open(initial));assert(fil_current.cltbl);ok(audio_file_close());
    ok(audio_file_open("bank1/0.255.wav"));assert(!fil_current.cltbl);
    assert(seek_maps_request("bank1/0.255.wav")==SEEK_MAP_IDLE);
    BYTE data[8];UINT n;ok(f_read(&fil_current,data,sizeof data,&n));
    for(unsigned i=0;i<n;++i)assert(data[i]==pattern(i));ok(audio_file_close());
    reboot(&fs);unsigned writes=test_writes;ok(seek_maps_prepare(&fs,cid,test_sectors,initial));
    assert(!seek_maps_stats.builds&&!seek_maps_stats.writes&&test_writes==writes);
    assert(seek_maps_stats.reused==256&&seek_maps_stats.capacity==1);
    assert(seek_maps_unmount());ok(f_mount(NULL,"0:",0));
    puts("manifest cap: 256 persisted files, ID 65535, excess-file playback and unchanged reuse pass");
}
static void saved_selection(void) {
    SaveFile *source=SaveFile_malloc(),*target=SaveFile_malloc();
    source->bank=2;source->sample=7;source->fx_active[FX_TIMESTRETCH]=true;
    for(unsigned i=0;i<3;++i)for(unsigned j=0;j<16;++j) {
        source->sequencers[i][j]->rec_len=1;
        source->sequencers[i][j]->rec_key[0]=i*16+j;
    }
    assert(SaveFile_save(source,7));
    // Old firmware saved heap and callback addresses verbatim. They must be
    // ignored even when a new build places live objects at different addresses.
    FIL file;UINT n;BYTE poison[sizeof source->sequencers];memset(poison,0xa5,sizeof poison);
    ok(f_open(&file,"savefile7",FA_WRITE));ok(f_lseek(&file,offsetof(SaveFile,sequencers)));
    ok(f_write(&file,poison,sizeof poison,&n));assert(n==sizeof poison);
    for(unsigned i=0;i<48;++i) {
        ok(f_lseek(&file,sizeof(SaveFile)+i*sizeof(Sequencer)+offsetof(Sequencer,sequence_emit)));
        ok(f_write(&file,poison,sizeof(void *)*2,&n));
    }
    ok(f_close(&file));
    Sequencer *allocated[3][16];memcpy(allocated,target->sequencers,sizeof allocated);
    assert(SaveFile_load(target,7));
    assert(target->bank==2&&target->sample==7&&target->fx_active[FX_TIMESTRETCH]);
    assert(!memcmp(allocated,target->sequencers,sizeof allocated));
    for(unsigned i=0;i<3;++i)for(unsigned j=0;j<16;++j) {
        assert(target->sequencers[i][j]->rec_key[0]==i*16+j);
        assert(!target->sequencers[i][j]->sequence_emit&&!target->sequencers[i][j]->sequence_finished);
    }
    assert(!SaveFile_load(target,8));
    ok(f_open(&file,"savefile7",FA_WRITE));ok(f_lseek(&file,sizeof(SaveFile)+17));
    ok(f_truncate(&file));ok(f_close(&file));assert(!SaveFile_load(target,7));
    assert(!memcmp(allocated,target->sequencers,sizeof allocated));
    SaveFile_free(source);SaveFile_free(target);ok(f_unlink("savefile7"));
    puts("saved selection: existing layout loads without stale heap/code pointers; missing/truncated saves rejected");
}
static void restore_image(FATFS *fs,const BYTE *before) {
    assert(seek_maps_unmount());ok(f_mount(NULL,"0:",0));
    memcpy(test_disk,before,test_sectors*512);ok(f_mount(fs,"0:",1));
}
static void alter_index(unsigned sector,unsigned offset,DWORD value) {
    FIL file;BYTE data[512];UINT n;
    ok(f_open(&file,SEEK_MAP_DIRECTORY "/maps0.bin",FA_READ|FA_WRITE));
    ok(f_lseek(&file,sector*512));ok(f_read(&file,data,sizeof data,&n));assert(n==512);
    put32(data+offset,value);put32(data+508,seek_crc32(data,508));
    ok(f_lseek(&file,sector*512));ok(f_write(&file,data,sizeof data,&n));assert(n==512);
    ok(f_close(&file));
}
static void stable_three(FATFS *fs) {
    reboot(fs);unsigned writes=test_writes;prepare(fs);
    assert(seek_maps_stats.files==3&&seek_maps_stats.reused==3);
    assert(!seek_maps_stats.builds&&!seek_maps_stats.writes&&test_writes==writes);
}
static void compatibility_and_sync(void) {
    FATFS fs;test_disk_create(65536);BYTE work[4096];
    MKFS_PARM options={.fmt=FM_FAT|FM_SFD,.n_fat=1,.align=1,.au_size=512};
    ok(f_mkfs("0:",&options,work,sizeof work));ok(f_mount(&fs,"0:",1));
    ok(f_mkdir("bank1"));
    for(unsigned i=1;i<=3;++i) {char path[32];snprintf(path,sizeof path,"bank1/%u.0.wav",i);create_file(path,8193);}
    prepare(&fs);assert(seek_maps_stats.builds==3&&seek_maps_stats.commits==1);
    assert(seek_maps_unmount());
    size_t bytes=test_sectors*512;BYTE *before=malloc(bytes);assert(before);memcpy(before,test_disk,bytes);
    // Checksum-valid malformed data must never reach FatFs as a CLMT.
    const DWORD malformed[][2]={
        {36,0},{36,1},{36,3},{36,65},{36,0xffffffff},
        {128,6},{132,0},{136,1},{136,0xffffffff},{140,2},
        {14,0},{72,0},{104,2},{16,0xffffffff},{20,1}};
    for(unsigned trial=0;trial<sizeof malformed/sizeof *malformed;++trial) {
        restore_image(&fs,before);alter_index(2,malformed[trial][0],malformed[trial][1]);
        reboot(&fs);prepare(&fs);
        assert(seek_maps_stats.builds==1&&seek_maps_stats.reused==2&&!seek_maps_stats.failures);
        stable_three(&fs);
    }
    // Incompatible schema/policy in both headers invalidates this whole index.
    for(unsigned offset=4;offset<=8;offset+=4) {
        restore_image(&fs,before);alter_index(0,offset,2);alter_index(1,offset,2);
        reboot(&fs);prepare(&fs);
        assert(seek_maps_stats.builds==3&&!seek_maps_stats.reused);stable_three(&fs);
    }
    restore_image(&fs,before);uint8_t other_cid[16];memcpy(other_cid,cid,16);other_cid[15]=99;
    ok(seek_maps_prepare(&fs,other_cid,test_sectors,"bank1/1.0.wav"));
    assert(seek_maps_stats.builds==3&&!seek_maps_stats.reused);
    reboot(&fs);ok(seek_maps_prepare(&fs,other_cid,test_sectors,"bank1/1.0.wav"));
    assert(!seek_maps_stats.builds&&!seek_maps_stats.writes&&seek_maps_stats.reused==3);
    // Reformat with the same filenames, then restore the stale index as a
    // desktop full-card backup might do. The volume serial must distinguish it.
    restore_image(&fs,before);FIL file;UINT n;BYTE old_index[5*512];
    ok(f_open(&file,SEEK_MAP_DIRECTORY "/maps0.bin",FA_READ));
    ok(f_read(&file,old_index,sizeof old_index,&n));assert(n==sizeof old_index);ok(f_close(&file));
    ok(f_mount(NULL,"0:",0));++test_fattime;
    ok(f_mkfs("0:",&options,work,sizeof work));ok(f_mount(&fs,"0:",1));
    ok(f_mkdir("bank1"));ok(f_mkdir(SEEK_MAP_DIRECTORY));
    for(unsigned i=1;i<=3;++i) {char path[32];snprintf(path,sizeof path,"bank1/%u.0.wav",i);create_file(path,8193);}
    ok(f_open(&file,SEEK_MAP_DIRECTORY "/maps0.bin",FA_WRITE|FA_CREATE_ALWAYS));
    ok(f_write(&file,old_index,sizeof old_index,&n));assert(n==sizeof old_index);ok(f_close(&file));
    reboot(&fs);prepare(&fs);assert(seek_maps_stats.builds==3&&!seek_maps_stats.reused);stable_three(&fs);
    // Rename, replace with equal length, and truncate each affect one record.
    restore_image(&fs,before);ok(f_rename("bank1/2.0.wav","bank1/4.0.wav"));
    reboot(&fs);prepare(&fs);assert(seek_maps_stats.builds==1&&seek_maps_stats.reused==2);stable_three(&fs);
    restore_image(&fs,before);ok(f_unlink("bank1/2.0.wav"));
    create_file("filler.bin",8193);create_file("bank1/2.0.wav",8193);
    reboot(&fs);prepare(&fs);assert(seek_maps_stats.builds==1&&seek_maps_stats.reused==2);stable_three(&fs);
    restore_image(&fs,before);ok(f_open(&file,"bank1/2.0.wav",FA_WRITE));
    ok(f_lseek(&file,1025));ok(f_truncate(&file));ok(f_close(&file));
    reboot(&fs);prepare(&fs);assert(seek_maps_stats.builds==1&&seek_maps_stats.reused==2);stable_three(&fs);
    // A truncated record rebuilds only that unfinished file.
    restore_image(&fs,before);ok(f_open(&file,SEEK_MAP_DIRECTORY "/maps0.bin",FA_WRITE));
    ok(f_lseek(&file,sizeof old_index-173));ok(f_truncate(&file));ok(f_close(&file));
    reboot(&fs);prepare(&fs);assert(seek_maps_stats.builds==1&&seek_maps_stats.reused==2);stable_three(&fs);
    // A full interrupted checkpoint may contain many superseded records. Keep
    // its validated records and enforce the disk cap when another file arrives.
    restore_image(&fs,before);
    ok(f_open(&file,SEEK_MAP_DIRECTORY "/maps0.bin",FA_READ|FA_WRITE));
    BYTE records[3][512], zero[512]={0};
    ok(f_lseek(&file,1024));ok(f_read(&file,records,sizeof records,&n));assert(n==sizeof records);
    ok(f_lseek(&file,512));ok(f_write(&file,zero,512,&n));
    for(unsigned slot=0;slot<512;++slot) {
        ok(f_lseek(&file,(slot+2)*512));ok(f_write(&file,records[slot%3],512,&n));assert(n==512);
    }
    ok(f_close(&file));create_file("bank1/4.0.wav",8193);
    reboot(&fs);unsigned capped_writes=test_writes;prepare(&fs);
    assert(seek_maps_stats.builds==1&&seek_maps_stats.reused==3&&seek_maps_stats.capacity==1);
    assert(!seek_maps_stats.writes&&test_writes==capped_writes&&!seek_maps_stats.commits);
    assert(seek_maps_request("bank1/4.0.wav")==SEEK_MAP_IDLE);
    ok(audio_file_open("bank1/4.0.wav"));assert(fil_current.cltbl);ok(audio_file_close());
    assert(seek_maps_unmount());FILINFO capped;ok(f_stat(SEEK_MAP_DIRECTORY "/maps0.bin",&capped));
    assert(capped.fsize==514*512);
    puts("checkpoint cap: 512 slots retain validated records and bounded RAM fallback without further writes");
    // Lose all writes since the last successful device synchronization.
    // Unlike a write-error-only fixture, this rejects unsynchronized sectors.
    restore_image(&fs,before);create_file("bank1/4.0.wav",8193);create_file("bank1/5.0.wav",8193);
    memcpy(before,test_disk,bytes);test_durable_disk=malloc(bytes);assert(test_durable_disk);
    for(int failure=0;failure<16;++failure) {
        restore_image(&fs,before);memcpy(test_durable_disk,test_disk,bytes);
        test_sync_fail_after=failure;
        FRESULT result=seek_maps_prepare(&fs,cid,test_sectors,"bank1/1.0.wav");
        assert(result==FR_OK||result==FR_DISK_ERR);
        unsigned saved=seek_maps_stats.writes>2?seek_maps_stats.writes-2:0;if(saved>2)saved=2;
        assert(seek_maps_unmount());ok(f_mount(NULL,"0:",0));
        memcpy(test_disk,test_durable_disk,bytes);test_sync_fail_after=-1;
        ok(f_mount(&fs,"0:",1));prepare(&fs);
        assert(seek_maps_stats.builds==2-saved&&!seek_maps_stats.failures);
        reboot(&fs);unsigned writes=test_writes;prepare(&fs);
        assert(!seek_maps_stats.builds&&!seek_maps_stats.writes&&test_writes==writes);
    }
    assert(seek_maps_unmount());ok(f_mount(NULL,"0:",0));
    free(before);free(test_durable_disk);test_durable_disk=NULL;
    puts("compatibility: 15 malformed tables, schema/policy, CID, reformat, rename/replace/truncate and 16 sync-loss points pass");
}
static void supported_fragment_boundary(void) {
    FATFS fs;test_disk_create(65536);BYTE work[4096],data[512];
    MKFS_PARM options={.fmt=FM_FAT|FM_SFD,.n_fat=1,.align=1,.au_size=512};
    ok(f_mkfs("0:",&options,work,sizeof work));ok(f_mount(&fs,"0:",1));ok(f_mkdir("bank1"));
    for(unsigned variant=0;variant<2;++variant) {
        FIL file,gap;UINT n;char path[32];snprintf(path,sizeof path,"bank1/%u.0.wav",variant);
        ok(f_open(&file,path,FA_WRITE|FA_CREATE_ALWAYS));
        ok(f_open(&gap,"gap.bin",FA_WRITE|FA_CREATE_ALWAYS));
        for(unsigned fragment=0;fragment<31+variant;++fragment) {
            for(unsigned i=0;i<512;++i)data[i]=pattern(fragment*512+i);
            ok(f_write(&file,data,512,&n));assert(n==512);
            ok(f_write(&gap,data,512,&n));assert(n==512);
        }
        ok(f_close(&file));ok(f_close(&gap));
    }
    ok(seek_maps_prepare(&fs,cid,test_sectors,"bank1/0.0.wav"));
    assert(seek_maps_stats.builds==2&&seek_maps_stats.oversized==1);
    FIL mapped,ordinary;ok(f_open(&mapped,"bank1/0.0.wav",FA_READ));
    ok(f_open(&ordinary,"bank1/0.0.wav",FA_READ));assert(seek_maps_attach(&mapped,"bank1/0.0.wav"));
    assert(mapped.cltbl[0]==64);unsigned seed=7;
    for(unsigned i=0;i<1000;++i) {
        seed=seed*1664525+1013904223;
        // Alternate far-apart grain reads and arbitrary reverse/slice boundaries.
        unsigned offset=i%2?seed%512:30*512+seed%513;
        BYTE a[515],b[515];UINT na,nb;
        ok(f_lseek(&mapped,offset));ok(f_read(&mapped,a,sizeof a,&na));
        ok(f_lseek(&ordinary,offset));ok(f_read(&ordinary,b,sizeof b,&nb));
        assert(na==nb&&f_tell(&mapped)==f_tell(&ordinary)&&!memcmp(a,b,na));
    }
    seek_maps_detach(&mapped);ok(f_close(&mapped));ok(f_close(&ordinary));
    reboot(&fs);ok(seek_maps_prepare(&fs,cid,test_sectors,"bank1/0.0.wav"));
    assert(!seek_maps_stats.builds&&!seek_maps_stats.writes&&seek_maps_stats.reused==2);
    ok(audio_file_open("bank1/0.0.wav"));assert(fil_current.cltbl&&fil_current.cltbl[0]==64);ok(audio_file_close());
    ok(audio_file_open("bank1/1.0.wav"));assert(!fil_current.cltbl);ok(audio_file_close());
    assert(seek_maps_request("bank1/1.0.wav")==SEEK_MAP_IDLE);
    assert(seek_maps_unmount());ok(f_mount(NULL,"0:",0));
    puts("fragment cap: 31-fragment persisted map returns identical bytes; 32-fragment outcome reuses bounded fallback");
}
static void exercise(BYTE format, unsigned sectors, unsigned cluster_bytes, const char *name) {
    fprintf(stderr, "Testing %s\n", name);
    FATFS fs;
    test_disk_create(sectors);
    BYTE format_work[4096];
    MKFS_PARM options = {.fmt = format | FM_SFD, .n_fat = 1, .align = 1, .au_size = cluster_bytes};
    ok(f_mkfs("0:", &options, format_work, sizeof format_work));
    ok(f_mount(&fs, "0:", 1));
    test_fat_start = fs.fatbase; test_fat_end = fs.fatbase+fs.fsize;
    ok(f_mkdir("bank1"));
    FIL a, gap;
    ok(f_open(&a, "bank1/0.0.wav", FA_CREATE_ALWAYS | FA_WRITE));
    ok(f_open(&gap, "gap.bin", FA_CREATE_ALWAYS | FA_WRITE));
    unsigned chunk = fs.csize*512, total = chunk*180;
    BYTE *data = malloc(chunk); assert(data);
    UINT written;
    for (unsigned offset = 0; offset < total; offset += chunk) {
        for (unsigned i = 0; i < chunk; ++i) data[i] = pattern(offset+i);
        ok(f_write(&a, data, chunk, &written)); assert(written == chunk);
        ok(f_write(&gap, data, chunk, &written)); assert(written == chunk);
    }
    ok(f_close(&a)); ok(f_close(&gap)); free(data);
    FIL ordinary, mapped, preparation;
    ok(f_open(&ordinary, "bank1/0.0.wav", FA_READ));
    ok(f_lseek(&ordinary, 44));
    ok(f_open(&preparation, "bank1/0.0.wav", FA_READ));
    struct { DWORD table[4]; DWORD guard; } small = {{4}, 0xfeed1234};
    preparation.cltbl = small.table;
    assert(f_lseek(&preparation, CREATE_LINKMAP) == FR_NOT_ENOUGH_CORE);
    assert(small.guard == 0xfeed1234 && small.table[0] > 4);
    preparation.cltbl = NULL;
    DWORD table[512] = {512};
    preparation.cltbl = table;
    ok(f_lseek(&preparation, CREATE_LINKMAP));
    assert(table[0] == 362 && table[table[0]-1] == 0);
    inspect(&preparation,table);
    corrupt_link(&preparation,table[2+2*47]);
    assert(f_tell(&ordinary) == 44 && ordinary.err == 0 && ordinary.cltbl == NULL);
    preparation.cltbl = NULL; ok(f_close(&preparation));
    ok(f_open(&mapped, "bank1/0.0.wav", FA_READ));
    assert(mapped.cltbl == NULL); mapped.cltbl = table;
    unsigned normal_fat = 0, mapped_fat = 0, seed = 0x12345678;
    for (unsigned i = 0; i < 1500; ++i) {
        seed = seed*1664525u+1013904223u;
        FSIZE_t offset = i < 9 ? (FSIZE_t[]){0,44,511,512,chunk-1,chunk,total-1,total,total+33}[i]
                              : seed % (total+128);
        BYTE x[521], y[521]; UINT nx, ny;
        // Invalidate only the filesystem's shared read window so both paths see
        // equivalent cold metadata; the read-only files have no dirty window.
        fs.winsect = (LBA_t)-1; test_fat_reads = 0;
        ok(f_lseek(&ordinary, offset)); ok(f_read(&ordinary, x, sizeof x, &nx));
        normal_fat += test_fat_reads;
        fs.winsect = (LBA_t)-1; test_fat_reads = 0;
        ok(f_lseek(&mapped, offset)); ok(f_read(&mapped, y, sizeof y, &ny));
        mapped_fat += test_fat_reads;
        assert(nx == ny && f_tell(&ordinary) == f_tell(&mapped));
        assert(memcmp(x,y,nx) == 0);
        for (unsigned n = 0; n < nx; ++n) assert(x[n] == pattern(offset+n));
    }
    assert(normal_fat > 0 && mapped_fat == 0);
    mapped.cltbl = NULL; ok(f_close(&mapped)); ok(f_close(&ordinary));
    ok(f_open(&mapped, "bank1/0.0.wav", FA_READ)); assert(mapped.cltbl == NULL); ok(f_close(&mapped));
    contiguous(&fs);cache_index(&fs);
    if(format==FM_FAT32){persistence_faults(&fs);allocation_change(&fs);damaged_commit(&fs);saved_selection();}
    ok(f_mount(NULL, "0:", 0));
    printf("%s: 180 fragments, 1500 byte/position comparisons, ordinary FAT reads %u, mapped %u\n", name, normal_fat, mapped_fat);
}
int main(void) {
    cluster_order_arithmetic();
    hashes();
    exercise(FM_FAT, 3072, 512, "FAT12");
    exercise(FM_FAT, 65536, 512, "FAT16");
    exercise(FM_FAT32, 131072, 512, "FAT32");
    exercise(FM_EXFAT, 131072, 4096, "exFAT");
    bounded_workspace_and_full_media();
    manifest_capacity();
    compatibility_and_sync();
    supported_fragment_boundary();
    free(test_disk); test_disk = NULL;
}
