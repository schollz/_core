
void clock_in_do_update() {
  if (clock_in_activator < clock_ppqn -1) {
  clock_in_activator++;
  return; 
  } else {
    clock_in_activator = 0;
    clock_in_do = true;
    clock_in_last_last_time = clock_in_last_time;
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

void clock_handling_up(int time_diff) {
  // printf("[clockhandling] clock_handling_up: %d %d\n", time_diff, bpm_new);
  //clock_in_diff_2x = time_diff * 2;
  clock_in_diff_2x = time_diff * clock_ppqn;
  uint16_t bpm_new = round(60000000.0 / (float)(time_diff * clock_ppqn));
  if (bpm_new > 30 && bpm_new < 300) {
    sf->bpm_tempo = bpm_new;
  }
  clock_in_do_update();
}

void clock_handling_down(int time_diff) {
  // printf("[zeptocore] clock_handling_down: %d\n", time_diff);
}

void clock_handling_start() {
    // Use the clock_ppqn counter to determine when to trigger the clock event
    if (clock_in_activator < clock_ppqn - 1) {
        clock_in_activator++;
    } else {
        clock_in_activator = 0;  // reset counter when reaching the threshold
        clock_in_do = true;
        clock_in_last_last_time = clock_in_last_time;
        clock_in_last_time = time_us_32();
        clock_in_beat_total = 0;
        clock_in_ready = true;
        cancel_repeating_timer(&timer);
        do_restart_playback = true;
        timer_step();
        update_repeating_timer_to_bpm(sf->bpm_tempo);
    }
}

