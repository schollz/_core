#define debugf(format, ...) \
  printf(format " %s:%d\n", __VA_ARGS__, __FILE__, __LINE__)

// sample rate is defined by the codec, PCM5102
// blocks per second is defined by SAMPLES_PER_BUFFER
// which can be modified

#define MAX_VOLUME 255
#define BLOCKS_PER_SECOND SAMPLE_RATE / SAMPLES_PER_BUFFER
static int PHASE_DIVISOR = 4;

static const uint32_t PIN_DCDC_PSM_CTRL = 23;

// starts at splice start and ends at splice stop
#define PLAY_ONE_SHOT_STOP 0
// starts at splice start, and returns to start when reaching splice boundary
#define PLAY_ONE_SHOT_LOOP 1
// starts at splice start and ends at sample boundary
#define PLAY_THROUGH_STOP 2
// starts at splice start and returns to start when reaching sample boundary
#define PLAY_THROUGH_LOOP 3

// TODO: replace the play mode below with the play mode above
#define PLAY_MODE_CLASSIC 0
#define PLAY_MODE_ONESHOT_GO 1
#define PLAY_MODE_ONESHOT_STOP 2
