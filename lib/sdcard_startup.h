#ifdef INCLUDE_ZEPTOCORE
void printStringWithDelay2(char *str) {
  int len = strlen(str);
  for (int i = 0; i < len; i++) {
    char currentChar = str[i];
    for (int j = 0; j < sizeof(led_text_5x4) / sizeof(led_text_5x4[0]); j++) {
      if (led_text_5x4[j].character == currentChar) {
        int led = 0;
        for (int row = 0; row < 5; row++) {
          for (int col = 0; col < 4; col++) {
            bool is_one = (led_text_5x4[j].dots[row] >> (3 - col)) & 1;
            if (is_one) {
              LEDS_set(leds, led, LED_BRIGHT);
            } else {
              LEDS_set(leds, led, 0);
            }
            led++;
          }
          printf("\n");
        }
        printf("\n");
        LEDS_render(leds);
        if (currentChar == '.') {
          sleep_ms(50);
        } else {
          sleep_ms(200);
        }
        break;
      }
    }
  }
}
#endif

void flash_no_sdcard() {
  bool started = false;
#ifdef INCLUDE_ECTOCORE
  WS2812 *ws2812;
  ws2812 = WS2812_new(GPIO_WS2812, pio0, 2);
  WS2812_set_brightness(ws2812, 50);
#endif
  while (true) {
#ifdef INCLUDE_ECTOCORE
    for (uint8_t i = 0; i < 18; i++) {
      WS2812_fill(ws2812, i, 255, 0, 0);
    }
    WS2812_show(ws2812);
#endif
#ifdef INCLUDE_ZEPTOCORE
    LEDS_clear(leds);
    printStringWithDelay2("NO SDCARD");
#endif
    sleep_ms(500);
    started = run_mount();
    if (started) {
      // reset
      watchdog_reboot(0, SRAM_END, 0);
    }

#ifdef INCLUDE_ECTOCORE
    for (uint8_t i = 0; i < 18; i++) {
      WS2812_fill(ws2812, i, 0, 0, 0);
    }
    WS2812_show(ws2812);
#endif
#ifdef INCLUDE_ZEPTOCORE
    LEDS_clear(leds);
    printStringWithDelay2("NO SDCARD");
#endif
    sleep_ms(500);
    started = run_mount();
    if (started) {
      // reset
      watchdog_reboot(0, SRAM_END, 0);
    }
  }
}

int extractNumber(const char *str) {
  // Pointer to traverse the string
  const char *p = str;

  // Move the pointer to the end of the string
  while (*p != '\0') {
    p++;
  }

  // Move backwards until we find the first digit
  while (p >= str && !isdigit((unsigned char)*p)) {
    p--;
  }

  // Move backwards to get the entire number
  const char *numStart = p;
  while (numStart >= str && isdigit((unsigned char)*numStart)) {
    numStart--;
  }
  numStart++;  // Move one step forward to the first digit

  // Convert the substring to an integer
  return atoi(numStart);
}

void load_settings(const char *dir_name) {
  DIR dj;      /* Directory object */
  FILINFO fno; /* File information */
  FRESULT fr;  /* File result error */
  memset(&dj, 0, sizeof dj);
  memset(&fno, 0, sizeof fno);
  fr = f_findfirst(&dj, &fno, dir_name, "*");
  if (FR_OK != fr) {
    return;
  }
  while (fr == FR_OK && fno.fname[0]) { /* Repeat while an item is found */
    // check for the clock_start_stop_sync
    if (strcmp(fno.fname, "clock_stop_sync-off") == 0) {
      clock_start_stop_sync = false;
    } else if (strcmp(fno.fname, "clock_stop_sync-on") == 0) {
      clock_start_stop_sync = true;
    }

    // check for the mash_mode_momentary
    if (strcmp(fno.fname, "mash_mode_momentary-on") == 0) {
      mode_toggle_momentary = true;
    } else if (strcmp(fno.fname, "mash_mode_momentary-off") == 0) {
      mode_toggle_momentary = false;
    }

    // check for clock output trig/gate
    if (strcmp(fno.fname, "clock_output_trig-on") == 0) {
      clock_output_trig = true;
    } else if (strcmp(fno.fname, "clock_output_trig-off") == 0) {
      clock_output_trig = false;
    }

// switch data.SettingsOverrideWithReset {
//   case "none":
//     os.Create(path.Join(settingsFolder, "override_with_reset-none"))
//   case "sample":
//     os.Create(path.Join(settingsFolder, "override_with_reset-sample"))
//   case "break":
//     os.Create(path.Join(settingsFolder, "override_with_reset-break"))
//   case "amen":
//     os.Create(path.Join(settingsFolder, "override_with_reset-amen"))
//   case "clk":
//     os.Create(path.Join(settingsFolder, "override_with_reset-clk"))
//   }
// check for cv reset override
#ifdef INCLUDE_ECTOCORE
    if (strcmp(fno.fname, "override_with_reset-none") == 0) {
      cv_reset_override = CV_RESET_NONE;
    } else if (strcmp(fno.fname, "override_with_reset-sample") == 0) {
      cv_reset_override = CV_SAMPLE;
    } else if (strcmp(fno.fname, "override_with_reset-break") == 0) {
      cv_reset_override = CV_BREAK;
    } else if (strcmp(fno.fname, "override_with_reset-amen") == 0) {
      cv_reset_override = CV_AMEN;
    } else if (strcmp(fno.fname, "override_with_reset-clk") == 0) {
      cv_reset_override = CV_CLOCK;
    }
#endif

    // check for clock output behavior (sync with tempo or sync with slice)
    if (strcmp(fno.fname, "clock_behavior_sync_slice-on") == 0) {
      clock_behavior_sync_slice = true;
    } else if (strcmp(fno.fname, "clock_behavior_sync_slice-off") == 0) {
      clock_behavior_sync_slice = false;
    }

    // check for amen cv polarity
    if (strcmp(fno.fname, "amen_cv-unipolar") == 0) {
      global_amen_cv_bipolar = false;
    } else if (strcmp(fno.fname, "amen_cv-bipolar") == 0) {
      global_amen_cv_bipolar = true;
    }

    // check for amen cv behavior
    if (strcmp(fno.fname, "amen_behavior-jump") == 0) {
      global_amen_cv_behavior = AMEN_CV_BEHAVIOR_JUMP;
    } else if (strcmp(fno.fname, "amen_behavior-repeat") == 0) {
      global_amen_cv_behavior = AMEN_CV_BEHAVIOR_REPEAT;
    } else if (strcmp(fno.fname, "amen_behavior-split") == 0) {
      global_amen_cv_behavior = AMEN_CV_BEHAVIOR_SPLIT;
    }

    // check for break cv polarity
    if (strcmp(fno.fname, "break_cv-unipolar") == 0) {
      global_break_cv_bipolar = false;
    } else if (strcmp(fno.fname, "break_cv-bipolar") == 0) {
      global_break_cv_bipolar = true;
    }

    // check for sample cv polarity
    if (strcmp(fno.fname, "sample_cv-unipolar") == 0) {
      global_sample_cv_bipolar = false;
    } else if (strcmp(fno.fname, "sample_cv-bipolar") == 0) {
      global_sample_cv_bipolar = true;
    }

    // check if a file has the prefix "brightness"
    if (strncmp(fno.fname, "brightness-", 11) == 0) {
      global_brightness = extractNumber(fno.fname);
      if (global_brightness > 100) {
        global_brightness = 100;
      }
      printf("[sdcard_startup] '%s' brightness: %d\n", fno.fname,
             global_brightness);
    }

    // check for the clock_start_stop_sync
    if (strcmp(fno.fname, "knobx_select_sample-on") == 0) {
      global_knobx_sample_selector = true;
    } else if (strcmp(fno.fname, "knobx_select_sample-off") == 0) {
      global_knobx_sample_selector = false;
    }

    fr = f_findnext(&dj, &fno); /* Search for next item */
  }
  f_closedir(&dj);

  // go through directory grimoire/rune1/*
  // go through runes[1-7]
  for (uint8_t rune = 1; rune <= 7; rune++) {
    for (uint8_t effect = 0; effect < 16; effect++) {
      grimoire_rune_effect[rune - 1][effect] = false;
    }
    char dirname[32];
    if (strlen(dir_name) == 0) {
      sprintf(dirname, "grimoire/rune%d", rune);
    } else {
      sprintf(dirname, "%s/grimoire/rune%d", dir_name, rune);
    }
    printf("[sdcard_startup] checking %s\n", dirname);
    fr = f_findfirst(&dj, &fno, dirname, "*");
    if (FR_OK != fr) {
      // folder not found, create random effects
      for (uint8_t effect = 0; effect < 16; effect++) {
        grimoire_rune_effect[rune - 1][effect] = random_integer_in_range(0, 1);
      }
      continue;
    }
    // printf("[sdcard_startup] found %s\n", dirname);
    while (fr == FR_OK && fno.fname[0]) { /* Repeat while an item is found */
      // printf("[sdcard_startup] %s", fno.fname);
      // set to true if effect%d-on is found
      for (uint8_t effect = 1; effect <= 16; effect++) {
        char effect_name_on[100];
        char effect_name_off[100];
        sprintf(effect_name_on, "effect%d-on", effect);
        sprintf(effect_name_off, "effect%d-off", effect);
        if (strcmp(fno.fname, effect_name_on) == 0) {
          grimoire_rune_effect[rune - 1][effect - 1] = true;
          // printf("[%d][%d]=%d\n", rune - 1, effect - 1,
          //        grimoire_rune_effect[rune - 1][effect - 1]);
          break;
        } else if (strcmp(fno.fname, effect_name_off) == 0) {
          grimoire_rune_effect[rune - 1][effect - 1] = false;
          // printf("[%d][%d]=%d\n", rune - 1, effect - 1,
          //        grimoire_rune_effect[rune - 1][effect - 1]);
          break;
        }
      }
      // move to next file
      fr = f_findnext(&dj, &fno); /* Search for next item */
    }
  }
  f_closedir(&dj);
}

void check_setup_files() {
  DIR dj;      /* Directory object */
  FILINFO fno; /* File information */
  FRESULT fr;  /* File result error */

  // initialize save file
  for (uint8_t i = 0; i < 16; i++) {
    savefile_has_data[i] = false;
  }

  memset(&dj, 0, sizeof dj);
  memset(&fno, 0, sizeof fno);
  fr = f_findfirst(&dj, &fno, "", "*");
  if (FR_OK != fr) {
    return;
  }
  while (fr == FR_OK && fno.fname[0]) { /* Repeat while an item is found */
    // create savefile name
    for (uint8_t i = 0; i < 16; i++) {
      char savefile_name[100];
      sprintf(savefile_name, "savefile%d", i);
      if (strcmp(fno.fname, savefile_name) == 0) {
        printf("[sdcard_startup] found savefile%d\n", i);
        savefile_has_data[i] = true;
      }
    }
    fr = f_findnext(&dj, &fno); /* Search for next item */
  }
  f_closedir(&dj);

  // load settings
  // sleep_ms(3000);
  // load from base of card (backwards compatibility)
  load_settings("");
  // loading from settings folder (if exists)
  load_settings("settings");
}

void update_reverb() {
  if (freeverb == NULL) {
    return;
  }
  uint8_t val = sf->fx_param[FX_EXPAND][0];
  if (val < 85) {
    FV_Reverb_set_roomsize(freeverb, val * Q16_16_1 / 85);
    FV_Reverb_set_damp(freeverb, Q16_16_1 - (val * Q16_16_1 / 85));
  } else if (val < 170) {
    val = val - 85;
    FV_Reverb_set_roomsize(freeverb, Q16_16_1 - (val * Q16_16_1 / 85));
    FV_Reverb_set_damp(freeverb, val * Q16_16_1 / 85);
  } else {
    val = val - 170;
    FV_Reverb_set_roomsize(freeverb, val * Q16_16_1 / 85);
    FV_Reverb_set_damp(freeverb, val * Q16_16_1 / 85);
  }
  FV_Reverb_set_wet(freeverb, sf->fx_param[FX_EXPAND][1] * Q16_16_1 / 255);
}

bool filter_was_activated = false;
void update_fx(uint8_t fx_num) {
#ifdef INCLUDE_MIDI
  if (midi_input_activated) {
    printf_sysex("fx=%d,%d", fx_num, sf->fx_active[fx_num]);
  }
#endif
  switch (fx_num) {
    case FX_REVERSE:
      phase_forward = !sf->fx_active[fx_num];
      break;
    case FX_SATURATE:
      Saturation_setActive(saturation, sf->fx_active[fx_num]);
      break;
    case FX_COMB:
      // printf("[update_fx] comb: %d\n", sf->fx_active[fx_num]);
      Comb_setActive(combfilter, sf->fx_active[fx_num],
                     sf->fx_param[FX_COMB][0], sf->fx_param[FX_COMB][1]);
      break;
    case FX_BEATREPEAT:
      if (sf->fx_active[fx_num]) {
#ifdef INCLUDE_ECTOCORE
        uint16_t samples = 30 * 44100 / sf->bpm_tempo;
        if (random_integer_in_range(1, 100) < 50) {
          samples = samples * 3 / 2;
        }
        if (random_integer_in_range(1, 100) < 50) {
          samples = samples * 3 / 4;
        }
        if (random_integer_in_range(1, 100) < 50) {
          samples = samples * 2;
        }
        BeatRepeat_repeat(beatrepeat, samples);
#else
        BeatRepeat_repeat(beatrepeat,
                          sf->fx_param[FX_BEATREPEAT][0] * 19000 / 255 + 100);
#endif
      } else {
        BeatRepeat_repeat(beatrepeat, 0);
      }
      break;
    case FX_DELAY:
#ifdef INCLUDE_ZEPTOCORE
      if (sf->fx_active[fx_num]) {
        uint32_t duration = 1000;
        uint8_t faster = 1;
        if (random_integer_in_range(0, 100) < 25) {
          faster = 2;
        } else if (random_integer_in_range(0, 100) < 25) {
          faster = 4;
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
        Delay_setDuration(delay, duration);
        Delay_setFeedbackf(delay,
                           (float)random_integer_in_range(500, 900) / 1000.0f);
      }
#endif
      Delay_setActive(delay, sf->fx_active[fx_num]);
      break;
    case FX_EXPAND:
      if (sf->fx_active[fx_num]) {
        update_reverb();
      }
      break;
    case FX_TIGHTEN:
      if (sf->fx_active[fx_num]) {
        Gate_set_amount(audio_gate, sf->fx_param[FX_TIGHTEN][0]);
      } else {
        Gate_set_amount(audio_gate, 255);
      }
      break;
    case FX_REPITCH:
      if (sf->fx_active[fx_num]) {
        Envelope2_reset(
            envelope_pitch, BLOCKS_PER_SECOND, Envelope2_update(envelope_pitch),
            linlin((float)sf->fx_param[FX_REPITCH][0], 0.0, 255.0, 0.5, 2.0),
            linlin((float)sf->fx_param[FX_REPITCH][1], 0.0, 255.0, 0.25, 4.0));
      } else {
        Envelope2_reset(
            envelope_pitch, BLOCKS_PER_SECOND, Envelope2_update(envelope_pitch),
            1.0,
            linlin((float)sf->fx_param[FX_REPITCH][1], 0.0, 255.0, 0.25, 4.0));
      }
      break;
    // case FX_SPEEDUP:
    //   if (sf->fx_active[fx_num]) {
    //     Envelope2_reset(envelope_pitch, BLOCKS_PER_SECOND,
    //                     Envelope2_update(envelope_pitch), 2.0, 1);
    //   } else {
    //     Envelope2_reset(envelope_pitch, BLOCKS_PER_SECOND,
    //                     Envelope2_update(envelope_pitch), 1.0, 1);
    //   }
    //   break;
    case FX_TAPE_STOP:
      if (sf->fx_active[FX_TAPE_STOP]) {
        Envelope2_reset(envelope_pitch, BLOCKS_PER_SECOND,
                        Envelope2_update(envelope_pitch),
                        ENVELOPE_PITCH_THRESHOLD / 2,
                        linlin((float)sf->fx_param[FX_TAPE_STOP][0], 0.0, 255.0,
                               0.15, 6.0));
      } else {
        if (sf->fx_active[FX_REPITCH]) {
          Envelope2_reset(
              envelope_pitch, BLOCKS_PER_SECOND,
              Envelope2_update(envelope_pitch),
              linlin((float)sf->fx_param[FX_REPITCH][0], 0.0, 255.0, 0.5, 2.0),
              linlin((float)sf->fx_param[FX_TAPE_STOP][1], 0.0, 255.0, 0.15,
                     6.0));
        } else {
          Envelope2_reset(envelope_pitch, BLOCKS_PER_SECOND,
                          Envelope2_update(envelope_pitch), 1.0,
                          linlin((float)sf->fx_param[FX_TAPE_STOP][1], 0.0,
                                 255.0, 0.15, 6.0));
        }
      }
      break;
    case FX_FUZZ:
      if (sf->fx_active[FX_FUZZ]) {
        printf("fuzz activated!\n");
      }
      break;
    case FX_FILTER:
      if (sf->fx_active[FX_FILTER] && !filter_was_activated) {
        // turn on filter
        filter_was_activated = true;
        printf("filter activated!\n");
        EnvelopeLinearInteger_reset(
            envelope_filter, BLOCKS_PER_SECOND,
            EnvelopeLinearInteger_update(envelope_filter, NULL),
            linlin(sf->fx_param[FX_FILTER][0], 0, 255, 5,
                   resonantfilter_fc_max),
            linlin(sf->fx_param[FX_FILTER][1], 0, 255, 0.5, 5));
      } else if (filter_was_activated) {
        // turn off filter
        printf("filter deactivated!\n");
        filter_was_activated = false;
        EnvelopeLinearInteger_reset(
            envelope_filter, BLOCKS_PER_SECOND,
            EnvelopeLinearInteger_update(envelope_filter, NULL),
            global_filter_index,
            linlin(sf->fx_param[FX_FILTER][1], 0, 255, 0.5, 5));
      }
      break;
    // case FX_VOLUME_RAMP:
    //   if (sf->fx_active[FX_VOLUME_RAMP]) {
    //     Envelope2_reset(envelope_volume, BLOCKS_PER_SECOND,
    //                     Envelope2_update(envelope_volume), 0, 1.618 / 2);
    //   } else {
    //     Envelope2_reset(envelope_volume, BLOCKS_PER_SECOND,
    //                     Envelope2_update(envelope_volume), 1, 1.618 / 2);
    //   }
    //   break;
    case FX_SCRATCH:
      scratch_lfo_val = 0;
      scratch_lfo_hz = sf->fx_param[FX_SCRATCH][0] / 255.0 * 4.0 + 0.1;
      scratch_lfo_inc = round(SCRATCH_LFO_1_HZ * scratch_lfo_hz);
      break;
    case FX_TIMESTRETCH:
      Gate_set_active(audio_gate, !sf->fx_active[FX_TIMESTRETCH]);
      if (sf->fx_active[FX_TIMESTRETCH]) {
        sel_variation_next = 1;
      } else {
        sel_variation_next = 0;
      }
      // fil_current_change = true;
      break;
    default:
      break;
  }
}

void fx_sequencer_emit(uint8_t key) {
#ifdef INCLUDE_MIDI
  // midi out
  MidiOut_on(midiout[4], key, 127);
#endif
  printf("[fx_sequencer_emit] key %d\n", key);
  if (key < 16) {
    sf->fx_active[key] = true;
    update_fx(key);
  } else if (key < 32) {
    sf->fx_active[key - 16] = false;
    update_fx(key - 16);
  }
}

void fx_sequencer_stop() { printf("[fx_sequencer_stop] stop\n"); }

void bass_sequencer_emit(uint8_t key) {
#ifdef INCLUDE_MIDI
  // midi out
  MidiOut_on(midiout[5], key, 127);
#endif
  printf("[bass_sequencer_emit] key %d\n", key);
#ifdef INCLUDE_SINEBASS
  if (key < 16) {
    WaveBass_note_on(wavebass, key);
  } else if (key < 32) {
    WaveBass_release(wavebass);
  }
#endif
}

void bass_sequencer_stop() { printf("[bass_sequencer_stop] stop\n"); }

void savefile_do_load() {
  if (savefile_has_data[savefile_current]) {
    while (sync_using_sdcard) {
      sleep_us(100);
    }
    sync_using_sdcard = true;
    SaveFile_load(sf, savefile_current);
    f_open(&fil_current, fil_current_name, FA_READ);
    sync_using_sdcard = false;
    printf("[button_handler] loading %s again\n", fil_current_name);
    // update all the fx
    for (uint8_t i = 0; i < 16; i++) {
      update_fx(i);
    }
    // update the sequencer callbacks
    for (uint8_t j = 0; j < 16; j++) {
      Sequencer_set_callbacks(sf->sequencers[0][j], step_sequencer_emit,
                              step_sequencer_stop);
    }
    for (uint8_t j = 0; j < 16; j++) {
      Sequencer_set_callbacks(sf->sequencers[1][j], fx_sequencer_emit,
                              fx_sequencer_stop);
    }
    for (uint8_t j = 0; j < 16; j++) {
      Sequencer_set_callbacks(sf->sequencers[2][j], bass_sequencer_emit,
                              bass_sequencer_stop);
    }

    // load new bank and sample
    sel_bank_next = sf->bank;
    sel_sample_next = sf->sample;
    printf("[SaveFile] loaded bank %d sample %d\n", sel_bank_next,
           sel_sample_next);
    fil_current_change = true;
  }
}

void sdcard_startup() {
  for (uint8_t i = SDCARD_CMD_GPIO - 1; i < SDCARD_D0_GPIO + 5; i++) {
    gpio_pull_up(i);
  }

  if (sdcard_startup_is_starting) {
    return;
  }
  sdcard_startup_is_starting = true;
  fil_is_open = false;
  while (sync_using_sdcard) {
    sleep_us(100);
  }
  sync_using_sdcard = true;
  bool started = false;
  for (uint8_t i = 0; i < 10; i++) {
    started = run_mount();
    if (started) {
      break;
    }
    sleep_ms(10);
  }
  if (!started) {
    printf("[sdcard_startup] could not mount\n");
    flash_no_sdcard();
    sleep_ms(1000000);
  }
  envelope_volume = Envelope2_create(BLOCKS_PER_SECOND, 0, 1, 4);
  envelope_pitch = Envelope2_create(BLOCKS_PER_SECOND, 0.5, 1.0, 1.5);
  envelope_filter = EnvelopeLinearInteger_create(BLOCKS_PER_SECOND, 1,
                                                 resonantfilter_fc_max, 0.3);
  noise_wobble = Noise_create(time_us_64(), BLOCKS_PER_SECOND);
  noise_feedback = Noise_create(time_us_64(), BLOCKS_PER_SECOND);
  noise_duration = Noise_create(time_us_64(), BLOCKS_PER_SECOND);
  audio_gate = Gate_create(BLOCKS_PER_SECOND, 165);
#ifdef INCLUDE_BASS
  bass = Bass_create();
#endif

  // sleep_ms(2000);
  check_setup_files();
  // sleep_ms(2000);

  for (uint8_t bi = 0; bi < 16; bi++) {
    // TODO: show which banks are loading?
    // #ifdef INCLUDE_ZEPTOCORE
    //     for (uint8_t i = 0; i < bi; i++) {
    //       LEDS_set(leds, i, 2);
    //     }
    //     LEDS_render(leds);
    // #endif

    char dirname[10];
    sprintf(dirname, "bank%d", bi + 1);
    banks[bi] = list_files(dirname);
    if (banks[bi]->num_samples > 0) {
      printf("[sdcard_startup] bank %d has %d samples\n", bi,
             banks[bi]->num_samples);
      banks_with_samples[banks_with_samples_num] = bi;
      banks_with_samples_num++;
      total_number_samples += banks[bi]->num_samples;
      for (uint8_t si = 0; si < banks[bi]->num_samples; si++) {
        continue;
        if (bi == 0) {
          for (uint8_t variation = 0; variation < 2; variation++) {
            printf(
                "[sdcard_startup] "
                "banks[%d]->sample[%d].snd[variation]->size: %d\n",
                bi, si, banks[bi]->sample[si].snd[variation]->size);
            printf(
                "[sdcard_startup] "
                "banks[%d]->sample[%d].snd[variation]->num_channels: %d\n",
                bi, si, banks[bi]->sample[si].snd[variation]->num_channels);
            printf(
                "[sdcard_startup] "
                "banks[%d]->sample[%d].snd[variation]->oversampling: %d\n",
                bi, si, banks[bi]->sample[si].snd[variation]->oversampling);
            printf(
                "[sdcard_startup] "
                "banks[%d]->sample[%d].snd[variation]->splice_trigger:% "
                "d\n",
                bi, si, banks[bi]->sample[si].snd[variation]->splice_trigger);
            printf(
                "[sdcard_startup] "
                "banks[%d]->sample[%d].snd[variation]->splice_variable:% "
                "d\n",
                bi, si, banks[bi]->sample[si].snd[variation]->splice_variable);
            printf(
                "[sdcard_startup] "
                "banks[%d]->sample[%d].snd[variation]->play_mode: "
                "% d\n ",
                bi, si, banks[bi]->sample[si].snd[variation]->play_mode);
            printf(
                "[sdcard_startup] "
                "banks[%d]->sample[%d].snd[variation]->bpm: "
                "%d\n",
                bi, si, banks[bi]->sample[si].snd[variation]->bpm);
            printf(
                "[sdcard_startup] "
                "banks[%d]->sample[%d].snd[variation]->slice_num: "
                "% d\n ",
                bi, si, banks[bi]->sample[si].snd[variation]->slice_num);
            printf("slices: \n");
            for (uint8_t i = 0;
                 i < banks[bi]->sample[si].snd[variation]->slice_num; i++) {
              printf("%d) %d-%d\n", i,
                     banks[bi]->sample[si].snd[variation]->slice_start[i],
                     banks[bi]->sample[si].snd[variation]->slice_stop[i]);
            }
            printf("\n");
          }
        }
      }
    }
  }  // bank loop

  // check to see if bank0/0.*2+x).wav exists
  // if it does, then we are in audio variant mode
  for (uint8_t i = 2; i < 16; i++) {
    char filename[100];
    sprintf(filename, "bank0/0.%d.wav", i);
    FILINFO fno;
    FRESULT fr = f_stat(filename, &fno);
    if (fr == FR_OK) {
      audio_variant_num = i;
      printf("[sdcard_startup] audio variant: %s\n", filename);
    }
  }
  if (audio_variant_num > 0) {
    audio_variant_num = (audio_variant_num - 1) / 2;
  }

  // // print transients
  // sleep_ms(2000);
  // printf("transient_num_1: %d\n",
  // banks[0]->sample[0].snd[0]->transient_num_1); for (uint8_t i = 0; i <
  // banks[0]->sample[0].snd[0]->transient_num_1; i++) {
  //   printf("transients[0][%d]: %d\n", i,
  //          banks[0]->sample[0].snd[0]->transients[0][i]);
  // }

#ifdef INCLUDE_ZEPTOCORE
  sample_selection = (SampleSelection *)malloc(sizeof(SampleSelection) * 255);
  for (uint8_t bi = 0; bi < 16; bi++) {
    for (uint8_t si = 0; si < banks[bi]->num_samples; si++) {
      // add to sample_selection list if not one-shot
      if (banks[bi]->sample[si].snd[0]->play_mode == PLAY_NORMAL &&
          sample_selection_num < 255) {
        // append to sample_selection
        sample_selection[sample_selection_num] =
            (SampleSelection){.bank = bi, .sample = si};
        sample_selection_num++;
      }
    }
  }
#endif

  // load save file
  // load new save file
  sf = SaveFile_malloc();

  // initialize sequencers
  for (uint8_t j = 0; j < 16; j++) {
    Sequencer_set_callbacks(sf->sequencers[0][j], step_sequencer_emit,
                            step_sequencer_stop);
  }
  for (uint8_t j = 0; j < 16; j++) {
    Sequencer_set_callbacks(sf->sequencers[1][j], fx_sequencer_emit,
                            fx_sequencer_stop);
  }
  for (uint8_t j = 0; j < 16; j++) {
    Sequencer_set_callbacks(sf->sequencers[2][j], bass_sequencer_emit,
                            bass_sequencer_stop);
  }
  // sync_using_sdcard = false;
  // SaveFile_save(sf, &sync_using_sdcard);
  // SaveFile_test_sequencer(sf);
  // SaveFile_load(sf);
  // SaveFile_test_sequencer(sf);
  // sleep_ms(3000);

  uint32_t total_heap = getTotalHeap();
  uint32_t used_heap = total_heap - getFreeHeap();
  printf("memory usage: %2.1f%% (%ld/%ld)\n",
         (float)(used_heap) / (float)(total_heap) * 100.0, used_heap,
         total_heap);

  // if you have too many samples, reverb won't work
  if (total_number_samples < 128) {
    // allocate as much space as possible for the reverb
    freeverb = FV_Reverb_malloc(FV_INITIALROOM, FV_INITIALDAMP, FV_INITIALWET,
                                FV_INITIALDRY);
  }

  total_heap = getTotalHeap();
  used_heap = total_heap - getFreeHeap();
  printf("memory usage: %2.1f%% (%ld/%ld)\n",
         (float)(used_heap) / (float)(total_heap) * 100.0, used_heap,
         total_heap);

  FRESULT fr;
  sprintf(fil_current_name, "bank%d/%d.%d.wav", sel_bank_cur + 1,
          sel_sample_cur, sel_variation + audio_variant * 2);

  fr = f_open(&fil_current, fil_current_name, FA_READ);
  if (fr != FR_OK) {
    printf("[sdcard_startup] could not open %s: %s\n", fil_current_name,
           FRESULT_str(fr));
  }
  sf->vol = 180;
  sf->pitch_val_index = PITCH_VAL_MID;
  phase_new = 0;
  phase_change = true;
  sync_using_sdcard = false;
  sdcard_startup_is_starting = false;

  savefile_do_load();

  fil_is_open = true;
  time_of_initialization = time_us_64();
}