// Copyright 2023-2025 Zack Scholl, GPLv3.0

#ifndef LIB_DEFINITIONS
#define LIB_DEFINITIONS 1

#define debugf(format, ...) \
  printf(format " %s:%d\n", __VA_ARGS__, __FILE__, __LINE__)

// sample rate is defined by the codec, PCM5102
// blocks per second is defined by SAMPLES_PER_BUFFER
// which can be modified

#define MAX_VOLUME 255
#define BLOCKS_PER_SECOND SAMPLE_RATE / SAMPLES_PER_BUFFER
static int PHASE_DIVISOR = 4;

static const uint32_t PIN_DCDC_PSM_CTRL = 23;

#define FILEZERO 0
static const int8_t sel_variation_scale[2] = {1, 8};

// shape
#define FX_SATURATE 0
#define FX_SHAPER 1
#define FX_FUZZ 2
#define FX_BITCRUSH 3
// time
#define FX_TIMESTRETCH 4
#define FX_DELAY 5
#define FX_COMB 6
#define FX_BEATREPEAT 7
// space
#define FX_TIGHTEN 8
#define FX_EXPAND 9
#define FX_PAN 10
#define FX_SCRATCH 11
// pitch
#define FX_FILTER 12
#define FX_REPITCH 13
#define FX_REVERSE 14
#define FX_TAPE_STOP 15

#define FX_TREMELO 21      // deactivated
#define FX_SPEEDUP 20      // deactivated
#define FX_VOLUME_RAMP 17  // deactivated
#define FX_WALTZ 18        // deactivated
#define FX_RETRIGGER 19    // deactivated

#define LED_NONE 0
#define LED_DIM 1
#define LED_BRIGHT 2
#define LED_BLINK 3

#endif