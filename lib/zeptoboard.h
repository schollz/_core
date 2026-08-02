#include "lib/midi_comm_callback.h"

void input_handling() {
  // flash bad signs
  while (!fil_is_open) {
    sleep_ms(10);
  }

#ifdef INCLUDE_MIDI
  tusb_init();
#endif

  while (1) {
#ifdef INCLUDE_MIDI
    tud_task();
    midi_comm_task(midi_comm_callback_fn, NULL, NULL, NULL, NULL, NULL, NULL,
                   NULL);
#endif

    // load the new sample if variation changed
    if (sel_variation_next != sel_variation) {
      if (!audio_callback_in_mute) {
        while (!sync_using_sdcard) {
          sleep_us(250);
        }
        while (sync_using_sdcard) {
          sleep_us(250);
        }
      }
      sync_using_sdcard = true;
      f_close(&fil_current);
      format_sample_filename(fil_current_name, sel_bank_cur, sel_sample_cur,
                             sel_variation_next + audio_variant * 2);
      f_open(&fil_current, fil_current_name, FA_READ);

      // TODO: fix this
      // if sel_variation_next == 0
      phases[0] = round(
          ((float)phases[0] * (float)sel_variation_scale[sel_variation_next]) /
          (float)sel_variation_scale[sel_variation]);

      sel_variation = sel_variation_next;
      sync_using_sdcard = false;
    }
  }
}