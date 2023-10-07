#define debugf(format, ...) \
  printf(format " %s:%d\n", __VA_ARGS__, __FILE__, __LINE__)

// sample rate is defined by the codec, PCM5102
// blocks per second is defined by SAMPLES_PER_BUFFER
// which can be modified
#ifdef INCLUDE_STEREO
#define WAV_CHANNELS 2
#endif
#ifndef INCLUDE_STEREO
#define WAV_CHANNELS 1
#endif

#define MAX_VOLUME 100
#define BLOCKS_PER_SECOND SAMPLE_RATE / SAMPLES_PER_BUFFER
static int PHASE_DIVISOR = WAV_CHANNELS * 2;

static const uint32_t PIN_DCDC_PSM_CTRL = 23;