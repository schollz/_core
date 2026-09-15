#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "pico/audio.h"
static bool locked;
static uint32_t spin_lock_blocking(spin_lock_t *lock) {
  (void)lock; assert(!locked); locked = true; return 0;
}
static void spin_unlock(spin_lock_t *lock, uint32_t irq) {
  (void)lock; (void)irq; assert(locked); locked = false;
}
void queue_free_audio_buffer(audio_buffer_pool_t *pool, audio_buffer_t *buffer) {
  assert(!locked); assert(!buffer->next);
  buffer->next = pool->free_list; pool->free_list = buffer;
}
static void __sev(void) {}
#include "audio_restart_queue.h"
#include "audio_restart.h"
static void test_queued_audio_is_preserved(void) {
  audio_buffer_pool_t pool = {0};
  audio_buffer_t leading_zero = {0}, music = {0}, trailing_zero = {0}, replacement = {0};
  leading_zero.flags = trailing_zero.flags = AUDIO_BUFFER_SILENCE;
  leading_zero.next = &music;
  music.next = &trailing_zero;
  pool.prepared_list = &leading_zero;
  pool.prepared_list_tail = &trailing_zero;
  assert(!audio_restart_can_wake(&pool));
  assert(audio_restart_publish_buffer(&pool, &replacement) == 1);
  assert(pool.free_list == &leading_zero && !leading_zero.next);
  assert(pool.prepared_list == &music && music.next == &trailing_zero);
  assert(trailing_zero.next == &replacement && !replacement.next);
  assert(pool.prepared_list_tail == &replacement);
}

int main(void) {
  test_queued_audio_is_preserved();
  audio_restart_arm(); assert(!audio_restart_pending());
  audio_restart_publish_phase(100); assert(audio_restart_pending());
  assert(!audio_restart_take(99)); assert(audio_restart_take(100));
  assert(!audio_restart_take(100));
  audio_restart_arm(); audio_restart_publish_phase(200);
  audio_restart_arm(); audio_restart_publish_phase(300);
  assert(!audio_restart_take(200)); assert(audio_restart_take(300));
  assert(!audio_restart_pending());
  assert(audio_restart_fade(-32768, 0) == 0);
  assert(audio_restart_fade(-32768, 16) == -16384);
  assert(audio_restart_fade(32767, 32) == 32767);
  int16_t pcm[512] = {0}; assert(audio_restart_pcm_silent(pcm, 256));
  pcm[511] = -1; assert(!audio_restart_pcm_silent(pcm, 256));
  audio_buffer_pool_t pool = {0};
  audio_buffer_t zero1 = {0}, zero2 = {0}, music = {0}, owned = {0}, replacement = {0};
  zero1.flags = zero2.flags = owned.flags = AUDIO_BUFFER_SILENCE;
  zero1.next = &zero2; zero2.next = &music;
  pool.prepared_list = &zero1; pool.prepared_list_tail = &music;
  assert(!audio_restart_can_wake(&pool)); // preserve audible queued audio
  zero2.next = NULL; pool.prepared_list_tail = &zero2;
  assert(!audio_restart_can_wake(&pool)); // more than one queued block: normal callback handles it
  // Readiness must NOT remove the fallback silence while rendering.
  assert(pool.prepared_list == &zero1 && !pool.free_list);
  assert(audio_restart_publish_buffer(&pool, &replacement) == 2);
  assert(pool.prepared_list == &replacement && pool.prepared_list_tail == &replacement);
  assert(!replacement.next && pool.free_list == &zero1 && zero1.next == &zero2 && !zero2.next);
  assert(!owned.next && owned.flags == AUDIO_BUFFER_SILENCE);
  // Simulate the consumer taking queued silence before replacement is ready.
  pool.prepared_list = pool.prepared_list_tail = &owned;
  assert(audio_restart_can_wake(&pool));
  pool.prepared_list = pool.prepared_list_tail = NULL;
  assert(!audio_restart_can_wake(&pool));
  assert(audio_restart_publish_buffer(&pool, &music) == 0);
  assert(pool.prepared_list == &music && pool.prepared_list_tail == &music);
  assert(!owned.next); // not returned to the free list: consumer still owns it
  assert(pool.free_list == &zero1);
  puts("restart: phase tokens, signed fade, exact silence, atomic replacement and consumer ownership pass");
}
