#include "disk.h"
#include "seek_fragment_benchmark.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void) {
    FATFS fs;BYTE work[4096],data[512];DWORD table[64];FIL file,spacing;
    test_disk_create(65536);
    MKFS_PARM options={.fmt=FM_FAT|FM_SFD,.n_fat=1,.align=1,.au_size=512};
    assert(!f_mkfs("0:",&options,work,sizeof work));assert(!f_mount(&fs,"0:",1));
    for(unsigned boot=0;boot<2;++boot) {
        if(boot) memset(&zeptocore_seek_benchmark,0,sizeof zeptocore_seek_benchmark);
        assert(!seek_fragment_benchmark(&fs,&file,&spacing,table,data));
        seek_benchmark_report *r=&zeptocore_seek_benchmark;
        if(r->context[1])for(unsigned i=0;i<15;++i)fprintf(stderr,"benchmark context[%u]=%u\n",i,r->context[i]);
        assert(r->complete==0x31424d53u&&!r->context[1]);
        assert(r->context[4]==31&&r->context[5]==64&&r->context[6]==768);
        assert(r->context[7]==!boot&&r->context[10]>0&&!r->context[11]);
        assert(r->context[12]==768*1024&&!r->context[13]);
        assert(r->ordinary_seek.count==768&&r->mapped_read.count==768);
        assert(f_open(&spacing,".core_seek_bench/spacing.tmp",FA_READ)==FR_NO_FILE);
    }
    assert(!f_mount(NULL,"0:",0));free(test_disk);test_disk=NULL;
    puts("fragment benchmark: 768 matched positions/bytes, 31 fragments, zero mapped FAT visits, existing fixture reused");
}
