#pragma once
typedef struct { unsigned unused; } spin_lock_t;
#define __dmb() __sync_synchronize()
