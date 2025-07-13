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
// int midi_note_mapping[20] = {19, 18, 17, 16, 12, 13, 14, 15, 8, 9, 10, 11, 4, 5, 6, 7, 0, 1, 2, 3 };
// int midi_note_mapping[20] = {19, 14, 9, 4, 
//                              18, 13, 8, 3, 
//                              17, 12, 7, 2, 
//                              16, 11, 6, 1, 
//                              15, 10, 5, 0};

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
// Midi notes!

// 52 53 54 55
// 48 49 50 51
// 44 45 46 47
// 40 41 42 43
// 36 37 39 39

// Midi FX:
// 75 74 73 72
// 71 70 69 68
// 67 66 65 64
// 60 61 61 60

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

// Effect :	Param 1	Param2	CC rng	CC 1	CC 2
// Saturation	pream		33	34	
// Loss	type	amount	35	36	37
// Fuzz	preamp	postamp	38	39	40
// Bitcrush	bits	freq	41	42	43
// Delay	feedback	duration	44	45	46
// Comb Filter	left	right	47	48	49
// Repeat	duration		50	51	
// Tighten	duration		52	53	
// Expand	expanse	dry/wet	54	55	56
// Circulate	rate	depth	57	58	59
// Scratch	rate		60	61	
// Lower	duration	depth	102	103	104
// Pitch	duration	depth	105	106	107
// Tapestop	duration		108	109	



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
    // case 33:
    //   sf->fx_param[0][2] = value * 255/127;
    //   break;
    // case 34:
    //   sf->fx_param[0][0] = value * 255/127;
    //   break;
    // case 35:
    //   sf->fx_param[1][2] = value * 255/127;
    //   break;
    // case 36:
    //   sf->fx_param[1][0] = value * 255/127;
    //   break;
    // case 37:
    //   sf->fx_param[1][1] = value * 255/127;
    //   break;
    // case 38:
    //   sf->fx_param[2][2] = value * 255/127;
    //   break;
    // case 39:
    //   sf->fx_param[2][0] = value * 255/127;
    //   break;
    // case 40:
    //   sf->fx_param[2][1] = value * 255/127;
    //   break;
    // case 41:
    //   sf->fx_param[3][2] = value * 255/127;
    //   break;
    // case 42:
    //   sf->fx_param[3][0] = value * 255/127;
    //   break;
    // case 43:
    //   sf->fx_param[3][1] = value * 255/127;
    //   break;
    // case 44:
    //   sf->fx_param[4][2] = value * 255/127;
    //   break;
    // case 45:
    //   sf->fx_param[4][0] = value * 255/127;
    //   break;
    // case 46:
    //   sf->fx_param[4][1] = value * 255/127;
    //   break;
    // case 47:
    //   sf->fx_param[5][2] = value * 255/127;
    //   break;
    // case 48:
    //   sf->fx_param[5][0] = value * 255/127;
    //   break;
    // case 49:
    //   sf->fx_param[5][1] = value * 255/127;
    //   break;
    // case 50:
    //   sf->fx_param[6][2] = value * 255/127;
    //   break;
    // case 51:
    //   sf->fx_param[6][0] = value * 255/127;
    //   break;
    // case 52:
    //   sf->fx_param[7][2] = value * 255/127;
    //   break;
    // case 53:
    //   sf->fx_param[7][0] = value * 255/127;
    //   break;
    // case 54:
    //   sf->fx_param[8][2] = value * 255/127;
    //   break;
    // case 55:
    //   sf->fx_param[8][0] = value * 255/127;
    //   break;
    // case 56:
    //   sf->fx_param[8][1] = value * 255/127;
    //   break;
    // case 57:
    //   sf->fx_param[9][2] = value * 255/127;
    //   break;
    // case 58:
    //   sf->fx_param[9][0] = value * 255/127;
    //   break;
    // case 59:
    //   sf->fx_param[9][1] = value * 255/127;
    //   break;
    // case 60:
    //   sf->fx_param[10][2] = value * 255/127;
    //   break;
    // case 61:
    //   sf->fx_param[10][0] = value * 255/127;
    //   break;
    // case 102:
    //   sf->fx_param[11][2] = value * 255/127;
    //   break;
    // case 103:
    //   sf->fx_param[11][0] = value * 255/127;
    //   break;
    // case 104:
    //   sf->fx_param[11][1] = value * 255/127;
    //   break;
    // case 105:
    //   sf->fx_param[12][2] = value * 255/127;
    //   break;
    // case 106:
    //   sf->fx_param[12][0] = value * 255/127;
    //   break;
    // case 107:
    //   sf->fx_param[12][1] = value * 255/127;
    //   break;
    // case 108:
    //   sf->fx_param[13][2] = value * 255/127;
    //   break;
    // case 109:
    //   sf->fx_param[13][0] = value * 255/127;
    //   break;

    //Volume tempo pitch
    // case 3:

    //   #ifdef INCLUDE_MIDI
    //   // send out midi cc
    //   MidiOut_cc(midiout[0], 3, value);
    //   #endif
    //   uint16_t bpm_new_tempo =
    //   banks[sel_bank_cur]->sample[sel_sample_cur].snd[FILEZERO]->bpm;
    //   bpm_new_tempo = round(
    //   linlin(value, 0, 127, bpm_new_tempo / 2, bpm_new_tempo * 3 / 2));
    //   if (bpm_new_tempo % 10 == 1 || bpm_new_tempo % 10 == 9) {
    //   // round to nearest 5
    //   bpm_new_tempo = (bpm_new_tempo / 5) * 5;
    //   } else if (bpm_new_tempo % 10 == 3 || bpm_new_tempo % 10 == 7) {
    //   // round to nearest 2
    //   bpm_new_tempo = (bpm_new_tempo / 2) * 2;
    //   }
    //   printf("bpm_new_tempo: %d\n", bpm_new_tempo);
    //   sf->bpm_tempo = util_clamp(bpm_new_tempo, 30, 300);
    //   break;
    // case 4:
    //   cc4 = value * 255/127;
    //   break;
    // case 5:
    //   cc5 = value * 255/127;
    //   break;

    // // random sequence, low pass, bass volume
    // case 6:
    //   #ifdef INCLUDE_MIDI
    //   // send out midi cc
    //   MidiOut_cc(midiout[0], 6, value);
    //   #endif
    //   // make_random_sequence(value * 255 / 127);  need to be put out of zeptocore.h
    //   break;
    // case 7:
    //   cc7 = value * 255/127;
    //   break;
    // case 8:
    //   cc8 = value * 255/127;
    //   break;

    // // sample select, tunelling, quantize sequence.
    // case 9:
    //   #ifdef INCLUDE_MIDI
    //   // send out midi cc
    //   MidiOut_cc(midiout[0], 9, value);
    //   #endif
    //   sample_selection_index = value * sample_selection_num / 127;
    //   printf("sample_selection_index: %d\n", sample_selection_index);
    //   break;
    // case 10:
    //   cc10 = value * 255/127;
    //   break;
    // case 11:
    //   cc11 = value * 255/127;
    //   break;  

    // // random jump, random effect, random effect bank
    // case 12:
    //   cc12 = value * 255/127;
    //   break;
    // case 13:
    //   cc13 = value * 255/127;
    //   break;
    // case 14:
    //   cc14 = value * 255/127;
    //   break;

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