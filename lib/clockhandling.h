
bool playback_was_stopped_clock = false;

void clock_handling_up(int time_diff) {
  // printf("[clockhandling] clock_handling_up: %d\n", time_diff);
  clock_in_diff_2x = time_diff * 2;
  uint16_t bpm_new = 60000000 / (time_diff * 2);
  if (sf->bpm_tempo - bpm_new > 2 || bpm_new - sf->bpm_tempo > 2) {
    sf->bpm_tempo = bpm_new;
  }
  if (clock_in_activator < 3) {
    clock_in_activator++;
  } else {
    clock_in_do = true;
    clock_in_last_time = time_us_32();
    clock_in_beat_total++;
    clock_in_ready = true;
  }
  if (playback_stopped) {
    playback_was_stopped_clock = true;
    clock_in_beat_total = 0;
  } else if (playback_was_stopped_clock) {
    playback_was_stopped_clock = false;
    clock_in_beat_total = 0;
  }
}

void clock_handling_down(int time_diff) {
  // printf("[zeptocore] clock_handling_down: %d\n", time_diff);
}

void clock_handling_start() {
  // printf("[clockhandling] clock_handling_start\n");
  if (clock_in_activator < 3) {
    clock_in_activator++;
  } else {
    clock_in_do = true;
    clock_in_last_time = time_us_32();
    clock_in_beat_total = 0;
    clock_in_ready = true;
    do_restart_playback = true;
  }
}