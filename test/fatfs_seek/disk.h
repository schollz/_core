#ifndef HOST_TEST_DISK_H
#define HOST_TEST_DISK_H
#include "ff.h"
#include "diskio.h"
extern unsigned char *test_disk;
extern LBA_t test_sectors;
extern unsigned test_reads, test_writes, test_syncs, test_fat_reads;
extern LBA_t test_fat_start, test_fat_end;
extern int test_read_fail_after, test_write_fail_after;
extern int test_sync_fail_after;
// Optional durable image: only successful CTRL_SYNC advances this snapshot.
extern unsigned char *test_durable_disk;
extern int test_readonly;
extern DWORD test_fattime;
void test_disk_create(LBA_t sectors);
#endif
