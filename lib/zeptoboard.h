#include "lib/midi_comm_callback.h"

void input_handling() {
  // flash bad signs
  while (!fil_is_open) {
    printf("waiting to start\n");
    sleep_ms(10);
  }

#ifdef INCLUDE_MIDI
  tusb_init();
#endif

  while (1) {
#ifdef INCLUDE_MIDI
    tud_task();
    midi_comm_task(midi_comm_callback_fn, NULL, NULL, NULL, NULL, NULL, NULL);
#endif

    if (MessageSync_hasMessage(messagesync)) {
      MessageSync_print(messagesync);
      MessageSync_clear(messagesync);
    }

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
      // measure the time it takes
      uint32_t time_start = time_us_32();
      FRESULT fr = f_close(&fil_current);
      if (fr != FR_OK) {
        debugf("[zeptocore] f_close error: %s\n", FRESULT_str(fr));
      }
      sprintf(fil_current_name, "bank%d/%d.%d.wav", sel_bank_cur,
              sel_sample_cur, sel_variation_next + tape_emulation * 2);
      fr = f_open(&fil_current, fil_current_name, FA_READ);
      if (fr != FR_OK) {
        debugf("[zeptocore] f_close error: %s\n", FRESULT_str(fr));
      }

      // TODO: fix this
      // if sel_variation_next == 0
      phases[0] = round(
          ((float)phases[0] * (float)sel_variation_scale[sel_variation_next]) /
          (float)sel_variation_scale[sel_variation]);

      sel_variation = sel_variation_next;
      sync_using_sdcard = false;
      printf("[zeptocore] loading new sample variation took %d us\n",
             time_us_32() - time_start);
    }
  }
}