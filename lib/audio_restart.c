#include "audio_restart.h"
#if AUDIO_RESTART_ENABLED
#include <stdatomic.h>
static bool armed; // clock/phase publisher on core 0 only
static uint32_t serial;
static _Atomic uint32_t expected_phase, requested, consumed;
void audio_restart_arm(void) { armed = true; }
void audio_restart_publish_phase(uint32_t phase) {
  if (!armed) return;
  atomic_store_explicit(&expected_phase, phase, memory_order_relaxed);
  atomic_store_explicit(&requested, ++serial, memory_order_release);
  armed = false;
}
bool audio_restart_pending(void) {
  return atomic_load_explicit(&requested, memory_order_acquire) !=
      atomic_load_explicit(&consumed, memory_order_relaxed);
}
bool audio_restart_take(uint32_t phase) {
  uint32_t token = atomic_load_explicit(&requested, memory_order_acquire);
  if (token == atomic_load_explicit(&consumed, memory_order_relaxed) ||
      phase != atomic_load_explicit(&expected_phase, memory_order_relaxed) ||
      token != atomic_load_explicit(&requested, memory_order_acquire)) return false;
  // Do not clear a newer request published while this block was starting.
  atomic_store_explicit(&consumed, token, memory_order_release);
  return true;
}
#endif
