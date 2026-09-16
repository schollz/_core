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

#include "midi_note_key.h"

void midi_start() {
  midi_timing_count = 24 * MIDI_RESET_EVERY_BEAT - 1;
  cancel_repeating_timer(&timer);
  do_restart_playback = true;
  timer_step();
  update_repeating_timer_to_bpm(sf->bpm_tempo);
  button_mute = false;
  trigger_button_mute = false;
}
void midi_continue() { midi_start(); }
void midi_stop() {
  midi_timing_count = 24 * MIDI_RESET_EVERY_BEAT - 1;
  trigger_button_mute = true;
  do_stop_playback = true;
}

void midi_control_change (uint8_t channel, uint8_t control, uint8_t value) {
  // printf("chan/cc/value: %d/%d/%d\n", channel, control, value);
  uint8_t new_adcvalue = (value * 2) ;
  switch (control) {
#if defined(SEEK_TEST_CONTROLS) && SEEK_TEST_CONTROLS
  // Explicit test firmware only. These invoke the ordinary musical controls;
  // the read-only SWD diagnostic service cannot send them or change playback.
  case 110:
    sf->fx_active[FX_REVERSE] = value != 0;
    update_fx(FX_REVERSE);
    break;
  case 111:
    key_do_jump_to_slice(value, value % 16);
    break;
  case 112:
    sf->fx_active[FX_TIMESTRETCH] = value != 0;
    update_fx(FX_TIMESTRETCH);
    break;
  case 113:
    set_audio_variant(value);
    break;
  case 114:
    // Exercise the ordinary acknowledged save/reopen path, only in an empty
    // preset slot. Never replace an existing preset through this test control.
    if(value<16&&!savefile_has_data[value]) {
      uint8_t previous=savefile_current;
      savefile_current=value;
      savefile_do_save();
      savefile_current=previous;
    }
    break;
  case 115:
    if(value<16&&savefile_has_data[value]) {
      uint8_t previous=savefile_current;
      savefile_current=value;
      savefile_do_load();
      savefile_current=previous;
    }
    break;
#endif
  case cc_volume: { // volume
      uint8_t new_vol = new_adcvalue; // is it 256 total?
      if (new_vol != sf->vol) {
        sf->vol = new_vol;
      }
      break;
  }
  case cc_realtime_stretch:
      set_realtime_stretch_knob((uint16_t)value * 4095 / 127);
      break;
  case cc_tempo: { // tempo
      uint8_t new_bpm = new_adcvalue ; // what is range?
      if (new_bpm != sf->bpm_tempo) {
        sf->bpm_tempo = new_bpm;
        // printf("sf-vol: %d\n", sf->vol);
      }
      break;
  }
  case cc_pitch: // pitch
          // if (adc < 2048 - 200) {
          //   sf->pitch_val_index = adc * PITCH_VAL_MID / (2048 - 200);
          // } else if (adc > 2048 + 200) {
          //   adc -= 2048 + 200;
          //   sf->pitch_val_index =
          //       adc * (PITCH_VAL_MAX - PITCH_VAL_MID) / (2048 - 200) +
          //       PITCH_VAL_MID;
          // } else {
          //   sf->pitch_val_index = PITCH_VAL_MID;
          // }

      if (new_adcvalue < 127 - 10) {
        sf->pitch_val_index = new_adcvalue * PITCH_VAL_MID / (127 - 10);
      } else if (new_adcvalue > 127 + 10) {
        new_adcvalue -= 127 + 10;
        sf->pitch_val_index =
          new_adcvalue * (PITCH_VAL_MAX - PITCH_VAL_MID) / (127 - 10) +
          PITCH_VAL_MID;
      } else {
        sf->pitch_val_index = PITCH_VAL_MID;
      }
      if (sf->pitch_val_index >= PITCH_VAL_MAX)
        sf->pitch_val_index = PITCH_VAL_MAX - 1;
      break;
  case cc_sampleselect: { // sample
    uint8_t new_sample = new_adcvalue ; // what is range?
    uint8_t sample_selection_index = 0;
    sample_selection_index = new_sample * (sample_selection_num - 1) / 255;
    uint8_t f_sel_bank_next = sample_selection[sample_selection_index].bank;
    uint8_t f_sel_sample_next = sample_selection[sample_selection_index].sample;
    if (f_sel_bank_next != sel_bank_cur ||
        f_sel_sample_next != sel_sample_cur) {
      sel_bank_next = f_sel_bank_next;
      sel_sample_next = f_sel_sample_next;
      ZD_CALL(zd_switch_request(((uint16_t)f_sel_bank_next<<12)|
          ((uint16_t)f_sel_sample_next<<8)|(sel_variation+audio_variant*2)));
      fil_current_change = true;
      }
      break;
  }
  case cc_quantize: { // Qunatize
    const uint8_t quantizations[10] = {1, 6, 12, 24, 48, 64, 96, 144, 192, 192};
      Sequencer_quantize(
          sf->sequencers[mode_buttons16][sf->sequence_sel[mode_buttons16]],
          quantizations[new_adcvalue * 9 / 255]);
      break;
  }
  case cc_randtunnel: // Random Tunnel
      probability_of_random_tunnel = new_adcvalue * 1000 / 256;
      // if (probability_of_random_tunnel < 100) {
      if ((new_adcvalue) < 20) {
        probability_of_random_tunnel = 0;
      }
      break;
  case cc_djfilter: // Filter
#ifdef INCLUDE_ZEPTOCORE
#if DJ_FILTER
      for (uint8_t channel = 0; channel < 2; channel++) {
        int filter_spacing = 16;
        for (uint8_t channel = 0; channel < 2; channel++) {
          if (new_adcvalue < 128 - filter_spacing) {
            global_filter_index =
              new_adcvalue * (resonantfilter_fc_max) / (128 - filter_spacing);
            global_filter_lphp = 0;
          ResonantFilter_setFilterType(resFilter[channel], global_filter_lphp);
            ResonantFilter_setFc(resFilter[channel], global_filter_index);
          } else if (value >= 64 + filter_spacing) {
            global_filter_index = (new_adcvalue - (128 + filter_spacing)) *
                      (resonantfilter_fc_max) /
                      (128 - filter_spacing);
            global_filter_lphp = 1;
          ResonantFilter_setFilterType(resFilter[channel], global_filter_lphp);
            ResonantFilter_setFc(resFilter[channel], global_filter_index);
          } else {
            global_filter_index = resonantfilter_fc_max;
            global_filter_lphp = 0;
          ResonantFilter_setFilterType(resFilter[channel], global_filter_lphp);
            ResonantFilter_setFc(resFilter[channel], resonantfilter_fc_max);
          }
        }
      }
#else
      set_global_lpf_cutoff(new_adcvalue, 255);
#endif
#else
      for (uint8_t channel = 0; channel < 2; channel++) {
        int filter_spacing = 16;
        for (uint8_t channel = 0; channel < 2; channel++) {
          if (new_adcvalue < 128 - filter_spacing) {
            global_filter_index =
              new_adcvalue * (resonantfilter_fc_max) / (128 - filter_spacing);
            global_filter_lphp = 0;
          ResonantFilter_setFilterType(resFilter[channel], global_filter_lphp);
            ResonantFilter_setFc(resFilter[channel], global_filter_index);
          } else if (value >= 64 + filter_spacing) {
            global_filter_index = (new_adcvalue - (128 + filter_spacing)) *
                      (resonantfilter_fc_max) /
                      (128 - filter_spacing);
            global_filter_lphp = 1;
          ResonantFilter_setFilterType(resFilter[channel], global_filter_lphp);
            ResonantFilter_setFc(resFilter[channel], global_filter_index);
          } else {
            global_filter_index = resonantfilter_fc_max;
            global_filter_lphp = 0;
          ResonantFilter_setFilterType(resFilter[channel], global_filter_lphp);
            ResonantFilter_setFc(resFilter[channel], resonantfilter_fc_max);
          }
        }
      }
#endif
      break;
  case cc_randfxbank: // Grimoire
      // change the grimoire rune
      grimoire_rune = new_adcvalue * 7 / 255;
      break;
  case cc_randfx: // Grimoire Probability = "Break"
      break_knob_set_point = new_adcvalue * 1024 / 255;
      break;
  case cc_randsequence: // Random sequencer
    if (new_adcvalue > 255)
      new_adcvalue = 255;
      if (new_adcvalue < 32) {
        // normal
        do_retrig_at_end_of_phrase = false;
        random_sequence_length = 0;
      } else if (new_adcvalue < 255 - 32) {
        do_retrig_at_end_of_phrase = false;
        uint8_t sequence_lengths[11] = {
            1, 2, 4, 6, 8, 12, 16, 24, 32, 48, 64,
        };
        random_sequence_length =
          sequence_lengths[((int16_t)(new_adcvalue - 32) * 11 / (255 - 32)) %
                           11];
      } else {
        // new random sequence
        regenerate_random_sequence_arr();
        random_sequence_length = 8;
        do_retrig_at_end_of_phrase = true;
      }
      break;
  case cc_randjump: // Random Jump - "Amen"
        if (new_adcvalue < 128) {
          sf->stay_in_sync = true;
          probability_of_random_jump = new_adcvalue * 100 / 128;
        } else if (new_adcvalue >= 128) {
          sf->stay_in_sync = false;
          probability_of_random_jump = (255 - new_adcvalue) * 100 / 128;
        }
      break;
  default:
    return;
  }
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
    clock_in_beat_total = -1;
    clock_in_beat_last = -1;
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
