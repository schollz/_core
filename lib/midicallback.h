#ifndef LIB_MIDI_CALLBACK_H
#define LIB_MIDI_CALLBACK_H 1

// #define DEBUG_MIDI 1
#define MIDI_MAX_NOTES 128
#define MIDI_MAX_TIME_ON 10000  // 10 seconds
#define MIDI_RESET_EVERY_BEAT 16
#define MIDI_CLOCK_MULTIPLIER 2

uint32_t note_hit[MIDI_MAX_NOTES];
int midi_bpm_detect[7];
uint8_t midi_bpm_detect_i = 0;
bool note_on[MIDI_MAX_NOTES];
uint32_t midi_last_time = 0;
uint32_t midi_delta_sum = 0;
uint32_t midi_delta_count = 0;
#define MIDI_DELTA_COUNT_MAX 32
uint32_t midi_timing_count = 0;
const uint8_t midi_timing_modulus = 24;
int fx_mapping[16] = {12, 13, 14, 15, 8, 9, 10, 11, 4, 5, 6, 7, 0, 1, 2, 3 };

int midi_note_mapping[20] = {
  0, 5, 10, 15,
  1, 6, 11, 16, 
  2, 7, 12, 17, 
  3, 8, 13, 18, 
  4, 9, 14, 19
                            };


void midi_note_off(int note) {
#ifdef DEBUG_MIDI
  printf("note_off: %d\n", note);
#endif
#ifdef INCLUDE_MIDICONTROLS

if(note > 35 && note < 56){
  midi_buttons[midi_note_mapping[note - 36]] = false;
}
if(note > 59 && note < 76){   // midi notes
  sf->fx_active[fx_mapping[note-60]] = false;
  if(fx_mapping[note-60] == fx_button){
    fx_button = -1;
  }
  update_fx(fx_mapping[note-60]);
}

#endif
}


void midi_note_on(int note, int velocity) {
#ifdef DEBUG_MIDI
  printf("note_on: %d\n", note);
#endif
#ifdef INCLUDE_MIDICONTROLS

if(velocity == 0){
  midi_note_off(note);
  return;
}
if(note > 35 && note < 56){   // midi notes
  midi_buttons[midi_note_mapping[note - 36]] = true;
}

if(note > 59 && note < 76){   // midi notes
  sf->fx_active[fx_mapping[note-60]] = true;
  fx_button = fx_mapping[note-60];
  update_fx(fx_mapping[note-60]);
}

#endif
}

void midi_cc(int control, int value) {
  #ifdef DEBUG_MIDI
    printf("cc: %d - %d\n", control, value);
  #endif
  switch (control) {
    case 16:
      midi_potx = value;
      break;
    case 17:
      midi_poty = value;
      break;
    case 18:
      midi_potz = value;
      break;


    default:
      break;
  }
}

void midi_start() {
#ifdef DEBUG_MIDI
  printf("[midicallback] midi start\n");
#endif
  midi_timing_count = 24 * MIDI_RESET_EVERY_BEAT - 1;
  cancel_repeating_timer(&timer);
  do_restart_playback = true;
  timer_step();
  update_repeating_timer_to_bpm(sf->bpm_tempo);
  button_mute = false;
  trigger_button_mute = false;
}
void midi_continue() {
#ifdef DEBUG_MIDI
  printf("[midicallback] midi continue (starting)\n");
#endif
  midi_start();
}
void midi_stop() {
#ifdef DEBUG_MIDI
  printf("[midicallback] midi stop\n");
#endif
  midi_timing_count = 24 * MIDI_RESET_EVERY_BEAT - 1;
  trigger_button_mute = true;
  do_stop_playback = true;
}

// Comparator function for qsort
int compare_ints(const void *a, const void *b) {
  return (*(int *)a - *(int *)b);
}

// Function to find the median of an array with 7 elements
int findMedian(int arr[], uint8_t size) {
  // Create a copy of the array to avoid modifying the original
  int arrCopy[size];
  memcpy(arrCopy, arr, sizeof(int) * size);

  // Sort the copy of the array
  qsort(arrCopy, size, sizeof(int), compare_ints);

  // Return the middle element from the sorted copy

  return arrCopy[size / 2];
}

void midi_timing() {
  midi_timing_count++;
  if (midi_timing_count % (24 * MIDI_RESET_EVERY_BEAT) == 0) {
    // reset
#ifdef DEBUG_MIDI
    printf("[midicallback] midi resetting\n");
#endif
    clock_in_beat_total = -1;
  }
  if (midi_timing_count % (midi_timing_modulus / MIDI_CLOCK_MULTIPLIER) == 0) {
    // soft sync
    clock_in_do_update();
  }
  uint32_t now_time = time_us_32();
  if (midi_last_time > 0) {
    midi_delta_sum += now_time - midi_last_time;
    midi_delta_count++;
    if (midi_delta_count == MIDI_DELTA_COUNT_MAX) {
      midi_bpm_detect[midi_bpm_detect_i] =
          (int)round(1250000.0 * MIDI_CLOCK_MULTIPLIER * MIDI_DELTA_COUNT_MAX /
                     (float)(midi_delta_sum));
      midi_bpm_detect_i++;
      if (midi_bpm_detect_i == 7) {
        midi_bpm_detect_i = 0;
      }
      // sort midi_bpm_detect

      int bpm_new = findMedian(midi_bpm_detect, 7);
      if (bpm_new > 60 && bpm_new < 260 && bpm_new != sf->bpm_tempo) {
        sf->bpm_tempo = bpm_new;
#ifdef DEBUG_MIDI
        printf("[midicallback] midi bpm = %d\n", bpm_new);
#endif
      }
      //   if (bpm_input - 7 != bpm_set) {
      //     // set bpm
      //   }
      midi_delta_count = 0;
      midi_delta_sum = 0;
    }
  }
  midi_last_time = now_time;
}
#endif