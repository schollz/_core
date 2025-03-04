// Copyright 2023-2024 Zack Scholl.
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
#include "hardware/flash.h"
#include "hardware/i2c.h"
#include "hardware/pio.h"
#include "hardware/pll.h"
#include "hardware/rtc.h"
#include "hardware/structs/clocks.h"
#include "hardware/sync.h"
#include "hardware/watchdog.h"
#include "pico/audio_i2s.h"
#include "pico/binary_info.h"
#include "pico/bootrom.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "pico/types.h"

//
#ifdef INCLUDE_MIDI
#include "bsp/board.h"
#include "tusb.h"
#endif
//
bool is_arcade_box = false;

//
#include "definitions.h"
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
#include "pcg_basic.h"
//
#include "memusage.h"
//
#include "fixedpoint.h"
#include "slew.h"
#include "utils.h"
#include "volume.h"
//
#if SAMPLES_PER_BUFFER == 128
#include "crossfade4_128.h"
#endif
#if SAMPLES_PER_BUFFER == 256
#include "crossfade4_256.h"
#endif
#if SAMPLES_PER_BUFFER == 441
#include "crossfade4_441.h"
#endif
//
#include "bitcrush.h"
#include "fuzz.h"
#include "saturation.h"
#include "shaper.h"
//
#include "random.h"
//
bool usb_midi_present = false;
#ifdef INCLUDE_MIDI
#include "midi_comm.h"
#include "midi_out.h"
#endif
//
#include "mcp23017/mcp23017_lib.h"
//
#include "array_resample.h"
#include "audio_pool.h"
#ifdef INCLUDE_BASS
#include "bass.h"
#endif
#ifdef INCLUDE_RGBLED
#include "WS2812.h"
#endif
#include "ads7830.h"
#include "beatrepeat.h"
#include "button_change.h"
#include "buttonmatrix3.h"
#include "charlieplex.h"
#include "clock_input.h"
#include "comb.h"
#ifdef INCLUDE_CUEDSOUNDS
#ifdef INCLUDE_ECTOCORE
#include "cuedsounds_ectocore.h"
#else
#include "cuedsounds_zeptocore.h"
#endif
#endif
#include "debounce.h"
#include "dust.h"
#include "envelope2_fp.h"
#include "envelope_linear_integer.h"
#include "file_list.h"
#include "filterexp.h"
#include "flashmem.h"
#include "freeverb_fp_mono.h"
#include "gate.h"
#include "knob_change.h"
#include "messagesync.h"
#include "sequencehandler.h"
#include "tapedelay.h"
#include "taptempo.h"
#ifdef INCLUDE_ECTOCORE
#include "dazzle.h"
#include "ectocore_easing.h"
#endif
#ifdef INCLUDE_ZEPTOCORE
#include "debounce_digits.h"
#include "led_text_5x4.h"
#include "leds2.h"
#include "ledtext.h"
#endif
#include "noise.h"
#include "resonantfilter.h"
#include "sdcard.h"
#ifdef INCLUDE_SINEBASS
#include "wavetablebass.h"
#endif
#include "wav.h"
//
#include "savefile.h"
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
#ifdef INCLUDE_ZEPTOCORE
#include "button_handler.h"
#endif