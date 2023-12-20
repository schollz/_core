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

#define debugf(format, ...) \
  printf(format " %s:%d\n", __VA_ARGS__, __FILE__, __LINE__)

// sample rate is defined by the codec, PCM5102
// blocks per second is defined by SAMPLES_PER_BUFFER
// which can be modified

#define MAX_VOLUME 255
#define BLOCKS_PER_SECOND SAMPLE_RATE / SAMPLES_PER_BUFFER
static int PHASE_DIVISOR = 4;

static const uint32_t PIN_DCDC_PSM_CTRL = 23;

#define FX_TIMESTRETCH 0
#define FX_SLOWDOWN 1
#define FX_NORMSPEED 2
#define FX_SPEEDUP 3
#define FX_VOLUME_RAMP_DOWN 4
#define FX_FILTER 5
#define FX_VOLUME_RAMP_UP 6
#define FX_SATURATE 8
#define FX_BITCRUSH 9
#define FX_TIGHTEN 10
#define FX_REVERSE 12
#define FX_TREMELO 13
#define FX_PAN 14
#define FX_TAPE_STOP 15
