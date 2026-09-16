// Copyright 2026 Zack Scholl, GPLv3.0
#ifndef SEEK_DIAGNOSTICS_H
#define SEEK_DIAGNOSTICS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "seek_switch_trace.h"

#define ZD_MAGIC 0x5a445031u
#define ZD_ABI 1u
#define ZD_BINS 16u
enum { ZD_AUDIO, ZD_CONTROL, ZD_IRQ, ZD_DOMAINS };
enum { ZD_SEEK, ZD_READ, ZD_STRETCH_SEEK, ZD_STRETCH_READ,
       ZD_OPEN, ZD_CLOSE, ZD_CALLBACK, ZD_AUDIO_METRICS };
enum { ZD_CONTROL_OPEN, ZD_CONTROL_CLOSE, ZD_STARTUP, ZD_CONTROL_METRICS };
enum { ZD_DMA_INTERVAL, ZD_NOTIFY_WAIT, ZD_IRQ_METRICS };
enum { ZD_SNAPSHOT = 1, ZD_MAP_LAYOUT = 2 };
enum { ZD_OK = 1, ZD_UNSUPPORTED = 2, ZD_MALFORMED = 3 };
enum { ZD_NO_BUFFER, ZD_MUTED, ZD_ERROR_SILENCE, ZD_SEEK_SKIPPED,
       ZD_CALLBACK_OVERRUN, ZD_FRAMES, ZD_SEEK_FORWARD, ZD_SEEK_BACKWARD,
       ZD_SEEK_SAME, ZD_SATURATED, ZD_PUBLISH_MAX_US, ZD_LAST_ERROR,
       ZD_FILE_GENERATION, ZD_AUDIO_RESERVED13, ZD_AUDIO_RESERVED14,
       ZD_AUDIO_RESERVED15 };

// All ABI members are 32-bit words; uint64 values use explicit low/high words.
typedef struct {
  uint32_t count, total_lo, total_hi, max_us, errors, short_reads;
  uint32_t bytes_lo, bytes_hi;
  uint32_t bins[ZD_BINS];
} zd_metric_t;
typedef struct {
  zd_metric_t metrics[ZD_AUDIO_METRICS];
  uint32_t counters[16];
  // bank, sample, variation, variant, direction, stretch Q8, pitch Q16,
  // BPM, effects bitmask, file size lo/hi, cluster, position lo/hi, map, ready.
  uint32_t context[16];
  // duration, from lo/hi, requested lo/hi, resulting lo/hi, result, kind,
  // file generation, source timestamp, mapped.
  uint32_t worst_seek[12];
} zd_audio_t;
typedef struct {
  zd_metric_t metrics[ZD_CONTROL_METRICS];
  uint32_t counters[16];
  // heap total/free, stage, then reserved identity/media fields.
  uint32_t context[32];
} zd_control_t;
typedef struct {
  zd_metric_t metrics[ZD_IRQ_METRICS];
  // dma count, missing count/frames, priming count/frames, notify timeouts,
  // consecutive failures, saturation, publication max, remaining reserved.
  uint32_t counters[16];
} zd_irq_t;
typedef struct {
  uint32_t sequence, command, arguments[6];
} zd_request_t;
typedef struct {
  uint32_t sequence, status, timestamp_us, progress, payload_bytes;
  uint32_t generation, publish_us, reserved;
} zd_response_header_t;
typedef struct {
  uint32_t header[32];
  zd_request_t request;
  struct { zd_response_header_t header; zd_audio_t data; } audio;
  struct { zd_response_header_t header; zd_control_t data; } control;
  struct { zd_response_header_t header; zd_irq_t data; } irq;
} zd_mailbox_t;

_Static_assert(sizeof(zd_metric_t) == 96, "diagnostic metric ABI");
_Static_assert(sizeof(zd_audio_t) == 848, "diagnostic audio ABI");
_Static_assert(sizeof(zd_control_t) == 480, "diagnostic control ABI");
_Static_assert(sizeof(zd_irq_t) == 256, "diagnostic IRQ ABI");
_Static_assert(sizeof(zd_audio_t) % 16 == 0 && sizeof(zd_control_t) % 16 == 0 &&
                   sizeof(zd_irq_t) % 16 == 0, "publication copy alignment");
_Static_assert(offsetof(zd_mailbox_t, request) == 128, "request ABI");

// These pure helpers are also compiled by host-side tests.
extern const uint32_t zd_bin_upper_us[ZD_BINS];
void zd_metric_record(zd_metric_t *metric, uint32_t us, uint32_t error,
                      uint32_t bytes, bool short_read, uint32_t *saturated);

#if defined(SEEK_DIAGNOSTICS) && SEEK_DIAGNOSTICS
#include "pico/time.h"
extern volatile zd_mailbox_t zeptocore_diag;
extern zd_audio_t zd_audio;
extern zd_control_t zd_control;
void zd_init(uint32_t clock_hz, uint32_t frames);
void zd_service(unsigned domain);
void zd_audio_begin(void);
void zd_audio_end(void);
void zd_audio_output(uint32_t frames, unsigned disposition);
void zd_audio_counter(unsigned counter);
void zd_record_io(unsigned kind, uint32_t start, uint32_t result,
                  uint32_t bytes, bool short_read);
void zd_record_seek(unsigned kind, uint32_t start, uint32_t result,
                    uint64_t from, uint64_t requested, uint64_t position,
                    bool mapped);
void zd_dma_start(bool missing, uint32_t frames);
void zd_notify(uint32_t start, bool success);
void zd_audio_clock(uint32_t clock_hz, uint32_t divider, uint32_t bits);
void zd_prepare_core1_stack(void);
#define ZD_TIME() time_us_32()
#define ZD_CALL(...) __VA_ARGS__
#else
#define ZD_TIME() 0u
#define ZD_CALL(...) ((void)0)
#endif
#endif
