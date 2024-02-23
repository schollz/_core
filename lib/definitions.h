// Copyright 2023 Zack Scholl.
//
// Author: Zack Scholl (zack.scholl@gmail.com)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// See http://creativecommons.org/licenses/MIT/ for more information.

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

#ifdef INCLUDE_ECTOCORE

#define KNOB_BREAK 3
#define KNOB_AMEN 0
#define KNOB_SAMPLE 6
#define ATTEN_BREAK 4
#define ATTEN_AMEN 2

#define GPIO_TAPTEMPO 2
#define GPIO_TAPTEMPO_LED 3
#endif

#endif