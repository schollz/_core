#ifndef AUDIO_RESTART_H
#define AUDIO_RESTART_H
#include <stdbool.h>
#include <stdint.h>
#ifndef AUDIO_CLOCK_RESTART_FIX
#define AUDIO_CLOCK_RESTART_FIX 0
#endif
#ifndef AUDIO_CLOCK_RESTART_WAKE
#define AUDIO_CLOCK_RESTART_WAKE 0
#endif
// Ezeptocore also defines INCLUDE_ECTOCORE and uses the same transport.
#if AUDIO_CLOCK_RESTART_FIX && defined(INCLUDE_ECTOCORE)
#define AUDIO_RESTART_ENABLED 1
#else
#define AUDIO_RESTART_ENABLED 0
#endif

// Silence replacement requires the direct 256-frame producer connection and
// its core-1 worker. Other connections retain ordinary DMA-driven rendering.
#if AUDIO_RESTART_ENABLED && AUDIO_CLOCK_RESTART_WAKE && \
    defined(SAMPLES_PER_BUFFER) && SAMPLES_PER_BUFFER == 256 && \
    defined(CORE1_PROCESS_I2S_CALLBACK)
#define AUDIO_RESTART_WAKE_ENABLED 1
#else
#define AUDIO_RESTART_WAKE_ENABLED 0
#endif

#if AUDIO_RESTART_ENABLED
#define AR_CALL(...) do { __VA_ARGS__; } while (0)
// Core 0 arms before publishing the restart phase; core 1 consumes it once.
void audio_restart_arm(void);
void audio_restart_publish_phase(uint32_t phase);
bool audio_restart_pending(void);
bool audio_restart_take(uint32_t phase);
#else
#define AR_CALL(...) ((void)0)
#endif
#define AUDIO_RESTART_FADE_FRAMES 32u
static inline int16_t audio_restart_fade(int16_t value, unsigned frame) {
  return frame < AUDIO_RESTART_FADE_FRAMES ?
      (int32_t)value * (int32_t)frame / (int32_t)AUDIO_RESTART_FADE_FRAMES : value;
}
#endif
