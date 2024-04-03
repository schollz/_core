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
    midi_comm_task(midi_comm_callback_fn);
#endif

    if (MessageSync_hasMessage(messagesync)) {
      MessageSync_print(messagesync);
      MessageSync_clear(messagesync);
    }
    sleep_ms(1);
  }
}