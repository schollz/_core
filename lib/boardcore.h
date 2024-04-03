void midi_comm_callback_fn(uint8_t status, uint8_t channel, uint8_t note,
                           uint8_t velocity) {
  // printf_sysex("status=%d, channel=%d, note=%d, vel=%d", status, channel,
  // note,
  //              velocity);

  int c = note;
  uint8_t key_to_jump[16] = {49, 50,  51,  52,  113, 119, 101, 114,
                             97, 115, 100, 102, 122, 120, 99,  118};
  uint8_t key_to_fx[16] = {53,  54,  55,  56,  116, 121, 117, 105,
                           103, 104, 106, 107, 98,  110, 109, 44};
  if (c >= 0) {
    for (int i = 0; i < 16; i++) {
      if (c == key_to_jump[i]) {
        key_do_jump(i);
      } else if (c == key_to_fx[i]) {
        sf->fx_active[i] = !sf->fx_active[i];
        update_fx(i);
      }
    }
  }
}

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