#include "disk.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
unsigned char *test_disk;
unsigned char *test_durable_disk;
LBA_t test_sectors;
unsigned test_reads, test_writes, test_syncs, test_fat_reads;
LBA_t test_fat_start, test_fat_end;
int test_read_fail_after = -1, test_write_fail_after = -1;
int test_sync_fail_after = -1;
int test_readonly;
DWORD test_fattime = ((2026u-1980u)<<25) | (9u<<21) | (15u<<16);
void test_disk_create(LBA_t sectors) {
    free(test_disk);free(test_durable_disk);test_durable_disk=NULL;
    test_disk = calloc(sectors, 512);
    assert(test_disk);
    test_sectors = sectors;
    test_reads = test_writes = test_syncs = test_fat_reads = 0;
    test_read_fail_after = test_write_fail_after = test_sync_fail_after = -1;
    test_readonly = 0;
}
DSTATUS disk_initialize(BYTE drive) { return disk_status(drive); }
DSTATUS disk_status(BYTE drive) {
    return drive || !test_disk ? STA_NODISK : test_readonly ? STA_PROTECT : 0;
}
DRESULT disk_read(BYTE drive, BYTE *buffer, LBA_t sector, UINT count) {
    if (drive || sector >= test_sectors || count > test_sectors-sector) return RES_PARERR;
    if (test_read_fail_after == 0) return RES_ERROR;
    if (test_read_fail_after > 0) --test_read_fail_after;
    ++test_reads;
    if (sector < test_fat_end && sector+count > test_fat_start) ++test_fat_reads;
    memcpy(buffer, test_disk+sector*512, count*512);
    return RES_OK;
}
DRESULT disk_write(BYTE drive, const BYTE *buffer, LBA_t sector, UINT count) {
    if (drive || sector >= test_sectors || count > test_sectors-sector) return RES_PARERR;
    if (test_readonly) return RES_WRPRT;
    if (test_write_fail_after == 0) return RES_ERROR;
    if (test_write_fail_after > 0) --test_write_fail_after;
    ++test_writes;
    memcpy(test_disk+sector*512, buffer, count*512);
    return RES_OK;
}
DRESULT disk_ioctl(BYTE drive, BYTE command, void *buffer) {
    if (drive) return RES_PARERR;
    switch (command) {
    case CTRL_SYNC:
        if(test_sync_fail_after==0)return RES_ERROR;
        if(test_sync_fail_after>0)--test_sync_fail_after;
        ++test_syncs;
        if(test_durable_disk)memcpy(test_durable_disk,test_disk,test_sectors*512);
        return RES_OK;
    case GET_SECTOR_COUNT: *(LBA_t *)buffer = test_sectors; return RES_OK;
    case GET_SECTOR_SIZE: *(WORD *)buffer = 512; return RES_OK;
    case GET_BLOCK_SIZE: *(DWORD *)buffer = 1; return RES_OK;
    default: return RES_PARERR;
    }
}
DWORD get_fattime(void) { return test_fattime; }
