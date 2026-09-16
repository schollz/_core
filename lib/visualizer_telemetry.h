// Copyright 2026 Zack Scholl, GPLv3.0
#ifndef VISUALIZER_TELEMETRY_H
#define VISUALIZER_TELEMETRY_H
#include <stdbool.h>
#include <stdint.h>

#if defined(INCLUDE_ZEPTOCORE) && defined(INCLUDE_MIDI)
#define ZV_ENABLED 1
#define ZV_CALL(...) do { __VA_ARGS__; } while (0)
#else
#define ZV_ENABLED 0
#define ZV_CALL(...) ((void)0)
#endif

typedef struct {
  uint8_t bank, sample, slice;
  uint16_t trigger, bpm;
  uint16_t effects;
  bool forward, stopped, muted, valid;
} zv_snapshot;

#if ZV_ENABLED
// Core 0 (foreground or timer IRQ): one atomic word records identity and a
// wrapping 16-bit trigger serial, including retriggers of the same slice.
void zv_trigger(uint8_t bank, uint8_t sample, uint8_t slice);
uint32_t zv_trigger_read(void);
// Clock/transport originates in the timer IRQ. TinyUSB's transmit mutex
// must only be entered by the foreground USB service.
void zv_realtime_enqueue(uint8_t status);
void zv_realtime_service(bool connected, bool (*write_packet)(const uint8_t[4]));

// Core 1 only, while media is owned. No USB, formatting, allocation or waits.
void zv_publish(const zv_snapshot *snapshot);
// Core 0: bounded read. False means a publication overlapped this attempt.
bool zv_read(zv_snapshot *snapshot);
bool zv_command(uint8_t status, uint8_t channel, uint8_t note,
                uint8_t velocity, uint32_t now);
bool zv_tx_pending(void);
// Foreground USB packets, retained across backpressure; at most 16 per call.
void zv_service(uint32_t now, bool connected, bool (*write_packet)(const uint8_t[4]));
#endif
#endif
