#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#define INCLUDE_ECTOCORE 1
#define GPIO_CLOCK_OUT 0
#define GPIO_TRIG_OUT 1
static bool global_startup_wait, do_restart_playback, do_stop_playback;
static bool playback_stopped, playback_restarted, button_mute, trigger_button_mute;
static bool repeating_timer_callback_playback_stopped, outputs[2];
static unsigned clock_output_trig_time, ecto_trig_out_last, releases;
static void gpio_put(unsigned pin, bool value) { outputs[pin] = value; }
#include "boot_transport.h"

static void audio_media_release(void) {
  ++releases;
  if (global_startup_wait) {
    assert(playback_stopped && button_mute && !do_restart_playback && !do_stop_playback);
    assert(!trigger_button_mute && !playback_restarted);
    assert(repeating_timer_callback_playback_stopped); // No boot Stop/flash-save transition.
    assert(!outputs[0] && !outputs[1]);
  } else assert(do_restart_playback && !button_mute);
}
static void boot(bool wait) {
  global_startup_wait = wait;
  playback_stopped = do_restart_playback = true;
  do_stop_playback = button_mute = trigger_button_mute = false;
  playback_restarted = repeating_timer_callback_playback_stopped = false;
  outputs[0] = outputs[1] = true;
#include "boot_release.h"
}
int main(void) {
  boot(false); boot(true); assert(releases == 2);
  puts("Firmware startup: silence established before media release; default restart preserved");
}
