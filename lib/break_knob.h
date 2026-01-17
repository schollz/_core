uint16_t break_knob_set_point = 0;

uint8_t break_fx_beat_refractory_min_max[32] = {
    4,  16,  // distortion
    4,  16,  // loss
    4,  16,  // bitcrush
    4,  16,  // filter
    16, 32,  // time stretch
    4,  16,  // delay
    4,  16,  // comb
    4,  8,   // beat repeat
    8,  32,  // reverb
    2,  6,   // autopan
    8,  32,  // pitch down
    8,  32,  // pitch up
    1,  8,   // reverse
    4,  8,   // retrigger no pitch
    8,  16,  // retrigger w/ pitch
    32, 64,  // tapestop
};
uint8_t break_fx_beat_duration_min_max[32] = {
    2, 4,   // distortion
    2, 4,   // loss
    2, 4,   // bitcrush
    4, 8,   // filter
    4, 32,  // time stretch
    4, 96,  // delay
    2, 6,   // comb
    1, 4,   // beat repeat
    8, 36,  // reverb
    4, 8,   // autopan
    8, 32,  // pitch down
    8, 32,  // pitch up
    1, 8,   // reverse
    4, 8,   // retrigger no pitch
    4, 12,  // retrigger w/ pitch
    8, 16,  // tape stop
};
uint8_t break_fx_probability_scaling[16] = {
    50,  // distortion
    50,  // loss
    50,  // bitcrush
    50,  // filter
    50,  // time stretch
    80,  // delay
    50,  // comb
    40,  // beat repeat
    40,  // reverb
    50,  // autopan
    40,  // pitch down
    30,  // pitch up
    90,  // reverse
    50,  // retirgger no pitch
    30,  // retrigger with pitch,
    10,  // tape sotp
};

uint8_t break_fx_beat_activated[16] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};
uint8_t break_fx_beat_after_activated[16] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

// if (random_integer_in_range(1, 2000000) < probability_of_random_retrig) {
//   printf("[ecotocre] random retrigger\n");
//   sf->do_retrig_pitch_changes = (random_integer_in_range(1, 10) < 5);
//   go_retrigger_2key(random_integer_in_range(0, 15),
//                     random_integer_in_range(0, 15));
// }

const uint8_t retrig_timer_dividers[15] = {8, 6, 4, 4, 4, 3, 2, 2,
                                           2, 2, 2, 2, 2, 1, 1};
void do_do_retrigger(uint8_t effect, bool on, bool pitch_changes) {
  // retrigger no pitch
  if (on && retrig_beat_num == 0) {
    sf->do_retrig_pitch_changes = pitch_changes;
    debounce_quantize = 0;
    retrig_first = true;
    retrig_beat_num = break_fx_beat_activated[effect];
    uint8_t divider = retrig_timer_dividers[random_integer_in_range(0, 14)];
    printf("retrig_beat_num=%d, divider=%d\n", retrig_beat_num, divider);
    retrig_timer_reset = 96 / divider;
    retrig_beat_num = retrig_beat_num * divider;
    if (retrig_beat_num == 0) {
      retrig_beat_num = 2;
    }
    retrig_vol = 1.0;
    if (random_integer_in_range(0, 100) < 25) {
      retrig_vol = 0.02;
      retrig_vol_step = ((float)random_integer_in_range(15, 50) / 100.0) /
                        ((float)retrig_beat_num);
    }
    if (random_integer_in_range(0, 100) < 50) {
      beat_current = beat_current_last;
    }
    retrig_ready = true;
  } else if (!on) {
    retrig_beat_num = 0;
    retrig_ready = false;
    retrig_vol = 1.0;
    retrig_pitch = PITCH_VAL_MID;
  }
}

void break_fx_toggle(uint8_t effect, bool on) {
  // if (effect != 4) {
  //   return;
  // }
  if (on) {
    // set the activation time
    break_fx_beat_activated[effect] =
        random_integer_in_range(break_fx_beat_duration_min_max[effect * 2],
                                break_fx_beat_duration_min_max[effect * 2 + 1]);
    // char fx_string[16];
    // for (uint8_t i = 0; i < 16; i++) {
    //   if (sf->fx_active[i]) {
    //     fx_string[i] = '1';
    //   } else {
    //     fx_string[i] = '0';
    //   }
    // }
    // printf("[break_fx_toggle] %s %d on %d\n", fx_string, effect + 1,
    //        break_fx_beat_activated[effect]);
  } else {
    // set the refractory period
    break_fx_beat_after_activated[effect] = random_integer_in_range(
        break_fx_beat_refractory_min_max[effect * 2],
        break_fx_beat_refractory_min_max[effect * 2 + 1]);
    // char fx_string[16];
    // for (uint8_t i = 0; i < 16; i++) {
    //   if (sf->fx_active[i]) {
    //     fx_string[i] = '1';
    //   } else {
    //     fx_string[i] = '0';
    //   }
    // }
    // printf("[break_fx_toggle] %s %d off %d \n", fx_string, effect + 1,
    //        break_fx_beat_after_activated[effect]);
    // print a string of 0's and 1's for each effect that is on
  }

  switch (effect) {
    case 0:
      // distortion
      if (on) {
        if (!sf->fx_active[FX_FUZZ]) {
          sf->fx_active[FX_FUZZ] = true;
          fuzz_auto_active = true;
        }
      } else {
        if (fuzz_auto_active) {
          sf->fx_active[FX_FUZZ] = false;
          fuzz_auto_active = false;
        }
      }
      update_fx(FX_FUZZ);
      break;
    case 1:
      // loss
      if (on) {
        sf->fx_param[FX_SHAPER][0] = random_integer_in_range(0, 255);
        sf->fx_param[FX_SHAPER][1] = random_integer_in_range(0, 255);
        sf->fx_active[FX_SHAPER] = true;
      } else {
        sf->fx_active[FX_SHAPER] = false;
      }
      update_fx(FX_SHAPER);
      break;
    case 2:
      // bitcrush
      if (on) {
        sf->fx_param[FX_BITCRUSH][0] = random_integer_in_range(220, 255);
        sf->fx_param[FX_BITCRUSH][1] = random_integer_in_range(210, 255);
        sf->fx_active[FX_BITCRUSH] = true;
      } else {
        sf->fx_active[FX_BITCRUSH] = false;
      }
      update_fx(FX_BITCRUSH);
      break;
    case 3:
      // filter
      if (on) {
        sf->fx_param[FX_FILTER][0] = random_integer_in_range(0, 128);
        sf->fx_param[FX_FILTER][1] = random_integer_in_range(0, 64);
        sf->fx_active[FX_FILTER] = true;
      } else {
        sf->fx_active[FX_FILTER] = false;
      }
      update_fx(FX_FILTER);
      break;
    case 4:
      // time stretch
      if (on) {
        sf->fx_active[FX_TIMESTRETCH] = true;
      } else {
        sf->fx_active[FX_TIMESTRETCH] = false;
      }
      update_fx(FX_TIMESTRETCH);
      break;
    case 5:
      // time-synced delay
      if (on) {
        uint8_t faster = 1;
        uint32_t duration = 1000;
        if (random_integer_in_range(0, 100) < 25) {
          faster = 2;
        }
        if (sf->bpm_tempo > 140) {
          duration = 30 * 44100 / sf->bpm_tempo / faster;
        }
        for (uint8_t i = 0; i < 4; i++) {
          if (duration > 10000) {
            duration = duration / 2;
          } else {
            break;
          }
        }
        uint8_t feedback = random_integer_in_range(1, 4);
        if (break_fx_beat_activated[effect] > 6) {
          feedback = 2;
        }
        Delay_setFeedback(delay, feedback);
        sf->fx_active[FX_DELAY] = true;
      } else {
        sf->fx_active[FX_DELAY] = false;
      }
      update_fx(FX_DELAY);
      break;
    case 6:
      // combo
      if (on) {
        sf->fx_param[FX_COMB][0] = random_integer_in_range(0, 120);
        sf->fx_param[FX_COMB][1] = random_integer_in_range(0, 20);
        sf->fx_active[FX_COMB] = true;
      } else {
        sf->fx_active[FX_COMB] = false;
      }
      update_fx(FX_COMB);
      break;
    case 7:
      // beat repeat
      sf->fx_active[FX_BEATREPEAT] = on;
      update_fx(FX_BEATREPEAT);
      break;
    case 8:
      // reverb
      sf->fx_active[FX_EXPAND] = on;
      update_fx(FX_EXPAND);
      break;
    case 9:
      // autopan
      sf->fx_active[FX_PAN] = on;
      uint8_t possible_speeds[3] = {2, 4, 8};
      lfo_pan_step =
          Q16_16_2PI / (48 * possible_speeds[random_integer_in_range(0, 2)]);
      update_fx(FX_PAN);
      break;
    case 10:
      // pitch down
      if (on && !sf->fx_active[FX_REPITCH]) {
        if (sf->bpm_tempo < 180 && sf->bpm_tempo > 120) {
          sf->fx_param[FX_REPITCH][0] = 0;
          sf->fx_param[FX_REPITCH][1] = random_integer_in_range(0, 100);
          sf->fx_active[FX_REPITCH] = on;
          update_fx(FX_REPITCH);
        }
      } else if (!on) {
        sf->fx_active[FX_REPITCH] = on;
        update_fx(FX_REPITCH);
      }
      break;
    case 11:
      // pitch up
      if (on && !sf->fx_active[FX_REPITCH]) {
        if (sf->bpm_tempo < 180 && sf->bpm_tempo > 120) {
          sf->fx_param[FX_REPITCH][0] = 255;
          sf->fx_param[FX_REPITCH][1] = random_integer_in_range(0, 100);
          sf->fx_active[FX_REPITCH] = on;
          update_fx(FX_REPITCH);
        }
      } else if (!on) {
        sf->fx_active[FX_REPITCH] = on;
        update_fx(FX_REPITCH);
      }
      break;
    case 12:
      // if (retrig_beat_num == 0) {
      // reverse
      sf->fx_active[FX_REVERSE] = on;
      update_fx(FX_REVERSE);
      // }
      break;
    case 13:
      // retrigger
      do_do_retrigger(effect, on, false);
      break;
    case 14:
      // retrigger pitched
      do_do_retrigger(effect, on, true);
      break;
    case 15:
      sf->fx_param[FX_TAPE_STOP][0] = random_integer_in_range(0, 128);
      sf->fx_param[FX_TAPE_STOP][1] = random_integer_in_range(0, 128);
      sf->fx_active[FX_TAPE_STOP] = on;
      update_fx(FX_TAPE_STOP);
      break;
    default:
      break;
  }
}

void break_fx_update() {
  if (!beat_did_activate) {
    return;
  }
  beat_did_activate = false;
  uint16_t break_knob_set_point_scaled =
      (((break_knob_set_point * break_knob_set_point) / 1024) *
       break_knob_set_point) /
      1024;
  // printf("[break_fx_update] %d\n", break_knob_set_point_scaled);
  for (uint8_t effect = 0; effect < 16; effect++) {
#ifdef INCLUDE_ECTOCORE
    // if break_knob is below 15 then turn off all effects
    if ((break_knob_set_point_scaled < 15 || debounce_file_change > 0) &&
        sf->fx_active[effect]) {
      // make the fx repitch turn off faster
      sf->fx_param[FX_REPITCH][1] = 0;
      break_fx_toggle(effect, false);
      continue;
    }
#endif
    // check if the fx is allowed in the grimoire runes
    if (grimoire_rune_effect[grimoire_rune][effect] == false &&
        break_fx_beat_activated[effect] > 0) {
      // turn off if it is activated
      break_fx_beat_activated[effect] = 0;
      break_fx_toggle(effect, false);
      continue;
    }
    if (break_fx_beat_activated[effect] > 0) {
      break_fx_beat_activated[effect]--;
      if (break_fx_beat_activated[effect] == 0) {
        // turn off the fx
        break_fx_toggle(effect, false);
      }
    } else if (break_fx_beat_after_activated[effect] > 0) {
      // don't allow to be turned on in this refractory period
      break_fx_beat_after_activated[effect]--;
    } else if (grimoire_rune_effect[grimoire_rune][effect] == true) {
      // roll a die to see if the fx is activated
      if (random_integer_in_range(0, 200) <
          break_knob_set_point_scaled * break_fx_probability_scaling[effect] /
              1024) {
        // activate the effect
        break_fx_toggle(effect, true);
      }
    }
  }
}
