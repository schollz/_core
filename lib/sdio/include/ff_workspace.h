// Bounded workspace for the firmware's exclusively owned FatFs instance.
#ifndef FF_WORKSPACE_H
#define FF_WORKSPACE_H
#include "ff.h"
#define FF_NAME_WORKSPACE_BYTES ((FF_MAX_LFN+1u)*2u + \
    (FF_FS_EXFAT ? ((FF_MAX_LFN+44u)/15u)*32u : 0u))
unsigned ff_workspace_reserved_bytes(void);
#endif
