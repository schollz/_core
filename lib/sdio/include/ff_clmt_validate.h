// Read-only adapter for the bundled FatFs R0.15 allocation rules.
#ifndef FF_CLMT_VALIDATE_H
#define FF_CLMT_VALIDATE_H
#include "ff.h"
typedef struct { DWORD clusters, fragments; BYTE digest[32]; } ff_clmt_info;
// table==NULL: inspect/hash the allocation chain without building a CLMT.
// A supplied table must describe the entire chain, including all interior links.
// Fresh read-only FIL only. Does not alter its position, error, or cltbl.
FRESULT ff_clmt_inspect(FIL *file, const DWORD *table, UINT words, ff_clmt_info *info);
#endif
