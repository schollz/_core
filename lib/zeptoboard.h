#include "lib/midi_comm_callback.h"

void input_handling() {

#ifdef INCLUDE_MIDI
  tusb_init();
#endif

  audio_media_boot_complete();
  while (1) {
    audio_media_poll();
#ifdef INCLUDE_MIDI
    tud_task();
    midi_comm_task(midi_comm_callback_fn, NULL, NULL, NULL, NULL, NULL, NULL,
                   NULL);
#endif

    // load the new sample if variation changed
    if (sel_variation_next != sel_variation) {
      audio_file_change_variation();
    }
  }
}
