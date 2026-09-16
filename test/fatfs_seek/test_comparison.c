// Comparison firmware still loads the same cache entries; only cltbl differs.
#include "disk.h"
#include "audio_seek_map.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
int main(void) {
    FATFS fs;BYTE work[4096],cid[16]={0};test_disk_create(65536);
    MKFS_PARM options={.fmt=FM_FAT|FM_SFD,.n_fat=1,.align=1,.au_size=512};
    assert(!f_mkfs("0:",&options,work,sizeof work));assert(!f_mount(&fs,"0:",1));
    assert(!f_mkdir("bank1"));
    for(unsigned i=0;i<3;++i) {
        FIL file;UINT n;char path[32];snprintf(path,sizeof path,"bank1/%u.0.wav",i);
        assert(!f_open(&file,path,FA_WRITE|FA_CREATE_ALWAYS));
        assert(!f_write(&file,"sample",6,&n)&&n==6);assert(!f_close(&file));
    }
    assert(!seek_maps_prepare(&fs,cid,test_sectors,"bank1/0.0.wav"));
    FIL file;assert(!f_open(&file,"bank1/1.0.wav",FA_READ));
    assert(!seek_maps_attach(&file,"bank1/1.0.wav")&&!file.cltbl);
    assert(seek_maps_stats.pending==SEEK_MAP_LOAD);seek_maps_service();
    assert(seek_maps_stats.loads==1&&!seek_maps_stats.pending);
    assert(!seek_maps_attach(&file,"bank1/1.0.wav")&&!file.cltbl&&!seek_maps_stats.pending);
    seek_maps_service();assert(seek_maps_stats.loads==1&&seek_maps_stats.builds==3);
    BYTE data[6];UINT n;assert(!f_read(&file,data,sizeof data,&n)&&n==6&&data[0]=='s');
    assert(!f_close(&file));assert(seek_maps_unmount());assert(!f_mount(NULL,"0:",0));
    free(test_disk);test_disk=NULL;
    puts("attachment-off comparison retains cache loads without attaching or repeatedly queuing work");
}
