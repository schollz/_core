uint8_t midi_comm_callback_last_jump = 0;
bool midi_comm_callback_do_retrigger = false;

void midi_comm_callback_fn(uint8_t status, uint8_t channel, uint8_t note,
                           uint8_t velocity) {
  // printf_sysex("status=%d, channel=%d, note=%d, vel=%d", status, channel,
  // note,
  //              velocity);
  // return;
  // 0x89
  if (!(status == 128 && channel == 9)) {
    return;
  }
  midi_input_activated = true;
  if (velocity == 1) {
    if (note == 93) {
      // bpm up
      sf->vol += 1;
      return;
    } else if (note == 91) {
      sf->vol -= 1;
      return;
    } else if (note == 61) {
      sf->bpm_tempo += 1;
      return;
    } else if (note == 46) {
      midi_comm_callback_do_retrigger = true;
      return;
    } else if (note == 45) {
      sf->bpm_tempo -= 1;
      return;
    } else if (note == 39) {
      for (uint8_t i = 1; i < 17; i++) {
        if (banks_with_samples[(sel_bank_cur + i) % 16] > 0) {
          sel_bank_next = (sel_bank_cur + i) % 16;
          sel_sample_next = sel_sample_cur % banks[sel_bank_next]->num_samples;
          fil_current_change = true;
          break;
        }
      }
      return;
    } else if (note == 47) {
      sel_sample_next++;
      if (sel_sample_next >= banks[sel_bank_next]->num_samples) {
        sel_sample_next = 0;
      }
      fil_current_change = true;
      return;
    } else if (note == 112) {
      // stop/play
      if (playback_stopped) {
        cancel_repeating_timer(&timer);
        do_restart_playback = true;
        timer_step();
        update_repeating_timer_to_bpm(sf->bpm_tempo);

        button_mute = false;
      } else {
        if (!button_mute) trigger_button_mute = true;
        do_stop_playback = true;
      }
    } else if (note == 59) {
      if (button_mute) {
        button_mute = false;
        trigger_button_mute = false;
      } else {
        trigger_button_mute = true;
      }
    }
    int c = note;
    uint8_t key_to_jump[16] = {49, 50,  51,  52,  113, 119, 101, 114,
                               97, 115, 100, 102, 122, 120, 99,  118};
    uint8_t key_to_fx[16] = {53,  54,  55,  56,  116, 121, 117, 105,
                             103, 104, 106, 107, 98,  110, 109, 44};
    for (int i = 0; i < 16; i++) {
      if (c == key_to_jump[i]) {
        if (midi_comm_callback_do_retrigger) {
          midi_comm_callback_do_retrigger = false;
#ifdef INCLUDE_ZEPTOCORE
          go_retrigger_2key(i, midi_comm_callback_last_jump);
#endif
        } else {
          key_do_jump(i);
          midi_comm_callback_last_jump = i;
        }
      } else if (c == key_to_fx[i]) {
        sf->fx_active[i] = !sf->fx_active[i];
        update_fx(i);
      }
    }
  } else {
    if (note == 3) {
      printf_sysex(
          "slices=%d",
          banks[sel_bank_cur]->sample[sel_sample_cur].snd[FILEZERO]->slice_num);
    } else if (note == 4) {
      printf_sysex("info=%d,%d,%d,%d,%d,%d,%d", sel_bank_cur, sel_sample_cur,
                   sf->bpm_tempo, sf->vol, midi_comm_callback_do_retrigger,
                   playback_stopped, button_mute);
    }
  }
}
