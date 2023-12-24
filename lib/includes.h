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

#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "hardware/adc.h"
#include "hardware/clocks.h"
#include "hardware/i2c.h"
#include "hardware/pio.h"
#include "hardware/pll.h"
#include "hardware/rtc.h"
#include "hardware/structs/clocks.h"
#include "pico/audio_i2s.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "pico/types.h"
//
#include "ff.h" /* Obtains integer types */
//
#include "diskio.h" /* Declarations of disk functions */
//
#include "f_util.h"
#include "hw_config.h"
#include "my_debug.h"
#include "sd_card.h"
//
#include "definitions.h"
#include "pcg_basic.h"
//
#include "array_resample.h"
#include "audio_pool.h"
#ifdef INCLUDE_BASS
#include "bass.h"
#endif
#ifdef INCLUDE_RGBLED
#include "WS2812.h"
#endif
#include "biquad.h"
#include "buttonmatrix3.h"
#include "chain.h"
#include "charlieplex.h"
#include "clock_input.h"
#include "envelope2_fp.h"
#include "envelope_linear_integer.h"
#include "file_list.h"
#include "filterexp.h"
#include "fixedpoint.h"
#include "gate.h"
#include "iir.h"
#include "leds.h"
#include "ledtext.h"
#include "noise.h"
#include "pca9552.h"
#include "random.h"
#include "resonantfilter.h"
#include "savefile.h"
#include "sdcard.h"
#include "selectx.h"
#include "sinewaves.h"
#include "wav.h"
//
#include "crossfade3.h"
#include "transfer_distortion.h"
#include "transfer_doublesine.h"
//
#include "globals.h"
//
#include "transfer.h"
//
#include "sdcard_startup.h"
//
#include "keyboard.h"
//
#include "audio_callback.h"
//
#include "button_handler.h"
