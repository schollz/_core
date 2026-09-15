#ifndef AUDIO_RESTART_QUEUE_H
#define AUDIO_RESTART_QUEUE_H
#include "pico/audio.h"

// Keep these helpers in flash instead of expanding them into the SRAM renderer.

// Check under the same lock used by the consumer. A restart may render while
// silence is queued, but must keep that silence available until replacement
// PCM is ready, or a near-boundary clock could cause a DMA underrun.
static __attribute__((noinline)) bool audio_restart_can_wake(audio_buffer_pool_t *pool) {
  uint32_t irq = spin_lock_blocking(pool->prepared_list_spin_lock);
  audio_buffer_t *buffer = pool->prepared_list;
  bool ready = buffer && !buffer->next && (buffer->flags & AUDIO_BUFFER_SILENCE);
  spin_unlock(pool->prepared_list_spin_lock, irq);
  return ready;
}

// Only for the ectocore 256-frame connection that queues producer PCM directly
// and converts on consumer take. Swap queued silence for finished PCM atomically.
// A consumer-held or DMA-owned buffer is absent from this list and untouched.
static __attribute__((noinline)) unsigned audio_restart_publish_buffer(audio_buffer_pool_t *pool, audio_buffer_t *replacement) {
  audio_buffer_t *discarded = NULL;
  unsigned removed = 0;
  uint32_t irq = spin_lock_blocking(pool->prepared_list_spin_lock);
  while (pool->prepared_list && (pool->prepared_list->flags & AUDIO_BUFFER_SILENCE)) {
    audio_buffer_t *buffer = pool->prepared_list;
    pool->prepared_list = buffer->next;
    if (!pool->prepared_list) pool->prepared_list_tail = NULL;
    buffer->next = discarded;
    discarded = buffer;
    ++removed;
  }
  replacement->next = NULL;
  if (pool->prepared_list_tail) pool->prepared_list_tail->next = replacement;
  else pool->prepared_list = replacement;
  pool->prepared_list_tail = replacement;
  spin_unlock(pool->prepared_list_spin_lock, irq);
  while (discarded) {
    audio_buffer_t *next = discarded->next;
    discarded->next = NULL;
    queue_free_audio_buffer(pool, discarded);
    discarded = next;
  }
  __sev();
  return removed;
}

static __attribute__((noinline)) bool audio_restart_pcm_silent(const int16_t *pcm, unsigned frames) {
  for (unsigned i = 0; i < frames * 2; ++i) {
    if (pcm[i]) return false;
  }
  return true;
}
#endif
