#include "disk.h"
#include "audio_seek_map.h"
#include "seek_audio_fixture.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void write_file(const char *name,const void *data,UINT size) {
    FIL file;UINT written;
    assert(!f_open(&file,name,FA_WRITE|FA_CREATE_NEW));
    assert(!f_write(&file,data,size,&written)&&written==size);assert(!f_close(&file));
}
int main(void) {
    FATFS fs;BYTE work[4096],original[2085],cid[16]={7};FIL file;UINT n;
    test_disk_create(65536);
    MKFS_PARM options={.fmt=FM_FAT|FM_SFD,.n_fat=1,.align=1,.au_size=512};
    assert(!f_mkfs("0:",&options,work,sizeof work));assert(!f_mount(&fs,"0:",1));
    assert(!f_mkdir("bank1"));
    for(unsigned i=0;i<sizeof original;++i)original[i]=(i*37)^(i>>9);
    write_file("bank1/0.0.wav",original,sizeof original);
    write_file("bank1/0.2.wav","existing",8);
    assert(seek_audio_fixture_run(1,cid)==FR_EXIST);
    assert(!f_open(&file,"bank1/0.2.wav",FA_READ)&&f_size(&file)==8);assert(!f_close(&file));
    assert(!seek_audio_fixture_run(2,cid)&&zeptocore_audio_fixture.operation==4);
    assert(!f_unlink("bank1/0.2.wav")); // test harness removes its collision fixture
    assert(!seek_maps_prepare(&fs,cid,test_sectors,"bank1/0.0.wav"));
    assert(!f_open(&file,"bank1/0.0.wav",FA_READ));assert(seek_maps_attach(&file,"bank1/0.0.wav"));
    assert(!f_lseek(&file,123));
    assert(!seek_audio_fixture_run(1,cid)&&zeptocore_audio_fixture.operation==1);
    assert(file.cltbl&&f_tell(&file)==123);seek_maps_detach(&file);assert(!f_close(&file));
    assert(!seek_audio_fixture_run(1,cid)&&zeptocore_audio_fixture.operation==2);
    assert(!seek_maps_prepare(&fs,cid,test_sectors,"bank1/0.0.wav"));
    assert(seek_maps_stats.builds==1&&seek_maps_stats.reused==1&&seek_maps_stats.files==2);
    assert(!f_open(&file,"bank1/0.2.wav",FA_READ));assert(seek_maps_attach(&file,"bank1/0.2.wav"));
    assert(seek_audio_fixture_run(2,cid)==FR_LOCKED);seek_maps_detach(&file);assert(!f_close(&file));
    cid[0]^=1;assert(seek_audio_fixture_run(2,cid)==FR_INVALID_PARAMETER);cid[0]^=1;
    assert(!f_open(&file,"bank1/0.2.wav",FA_WRITE));
    BYTE altered=original[0]^1;assert(!f_write(&file,&altered,1,&n)&&n==1);assert(!f_close(&file));
    assert(seek_audio_fixture_run(2,cid)==FR_INVALID_PARAMETER);
    assert(!f_open(&file,"bank1/0.2.wav",FA_WRITE));
    assert(!f_write(&file,original,1,&n)&&n==1);assert(!f_close(&file));
    assert(!seek_audio_fixture_run(2,cid)&&zeptocore_audio_fixture.operation==3);
    assert(f_open(&file,"bank1/0.2.wav",FA_READ)==FR_NO_FILE);
    assert(!seek_maps_prepare(&fs,cid,test_sectors,"bank1/0.0.wav"));
    assert(!seek_maps_stats.builds&&seek_maps_stats.reused==1&&seek_maps_stats.files==1);
    assert(!f_open(&file,"bank1/0.0.wav",FA_READ));
    assert(!f_read(&file,work,sizeof original,&n)&&n==sizeof original&&!memcmp(work,original,n));
    assert(!f_close(&file));
    write_file("bank1/0.1.wav",original,sizeof original);
    assert(!seek_audio_fixture_run(3,cid)&&zeptocore_audio_fixture.operation==1);
    assert(!f_open(&file,"bank1/0.3.wav",FA_READ)&&f_size(&file)==sizeof original);
    assert(!f_close(&file));
    assert(!seek_audio_fixture_run(4,cid)&&zeptocore_audio_fixture.operation==3);
    assert(f_open(&file,"bank1/0.3.wav",FA_READ)==FR_NO_FILE);
    assert(seek_maps_unmount());assert(!f_mount(NULL,"0:",0));
    free(test_disk);test_disk=NULL;
    puts("audio fixture: guarded add/reuse/remove, CID/content/pin protection, one changed-file build, source untouched");
}
