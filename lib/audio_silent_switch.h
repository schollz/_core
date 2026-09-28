// Complete a pending selection even when the renderer is outputting silence.
// Called only by the audio worker while it owns the playback filesystem.
#ifndef AUDIO_SILENT_SWITCH_H
#define AUDIO_SILENT_SWITCH_H
static void audio_switch_while_silent(void) {
  if (sync_using_sdcard ||
      !(fil_current_change || fil_current_change_force || do_open_file_ready))
    return;
  uint8_t bank = sel_bank_next, sample = sel_sample_next;
  if (bank >= 16 || !banks[bank] || !banks[bank]->num_samples) return;
  sample %= banks[bank]->num_samples;
  bool changed = bank != sel_bank_cur || sample != sel_sample_cur;
  bool reopen = changed || fil_current_change_force || do_open_file_ready;
  fil_current_change = false;
  if (!reopen) return;

  sync_using_sdcard = true;
  sel_bank_cur = bank;
  sel_sample_cur = sample;
  format_sample_filename(fil_current_name, bank, sample,
                         sel_variation + audio_variant * 2);
  FRESULT result = audio_file_open(fil_current_name);
  do_open_file_ready = fil_current_change_force = false;
  if (result == FR_OK) {
    // A stopped one-shot must not carry its exhausted slice into the new
    // selection. Keep explicit transport/mute/gate/envelope settings intact.
    phases[0] = phases[1] = phase_new = 0;
    phase_change = true;
    mute_because_of_playback_type = false;
    realtime_stretch_reset_from_playback_phase();
  }
  sync_using_sdcard = false;
}
#endif
