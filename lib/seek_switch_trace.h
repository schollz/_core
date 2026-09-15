// Copyright 2026 Zack Scholl, GPLv3.0
#ifndef SEEK_SWITCH_TRACE_H
#define SEEK_SWITCH_TRACE_H
#include <stdint.h>
#if defined(SEEK_DIAGNOSTICS) && SEEK_DIAGNOSTICS
#ifdef __cplusplus
extern "C" {
#endif
// One measurement in flight. Requests never wait; overlapping requests are
// counted as unmeasured. File keys use the shared canonical physical WAV ID.
void zd_switch_request(uint16_t key);
void zd_switch_file(uint32_t key);
uint32_t zd_switch_render_tag(void);
// user_data packs a 23-bit request token and a 9-bit consumer-frame offset.
uint32_t zd_switch_copy_tag(uint32_t destination, uint32_t source, uint32_t offset);
void zd_switch_dma(uint32_t tag, uint32_t started_us);
#ifdef __cplusplus
}
#endif
#endif
#endif
