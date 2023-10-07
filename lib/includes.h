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
#include "charlieplex.h"
#include "crossfade.h"
#include "envelope2.h"
#include "envelopegate.h"
#include "file_list.h"
#include "iir.h"
#include "leds.h"
#include "noise.h"
#include "pca9552.h"
#include "random.h"
#include "savefile.h"
#include "sdcard.h"
#include "selectx.h"
#include "wav.h"
//
#include "transfer_distortion.h"
#include "transfer_saturate.h"
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
