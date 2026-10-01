// Copyright 2026 Zack Scholl. SPDX-License-Identifier: GPL-3.0-only
// Explicit little-endian card decoding; no packed structs or host pointer ABI.
#pragma once
#include "core_engine.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    CoreSample sample;
    int32_t starts[255],stops[255];
    int8_t types[255];
    uint16_t transients[3][16];
} CoreCardInfo;
bool core_card_decode(const uint8_t *,size_t,CoreCardInfo *,char *error,size_t);
#ifdef __cplusplus
}
#endif
