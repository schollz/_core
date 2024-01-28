
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
    if (strcmp(fno.fname, "resample_linear") == 0) {
      quadratic_resampling = false;
      printf("[sdcard_startup] linear resampling\n");
    } else if (strcmp(fno.fname, "resample_quadratic") == 0) {
      quadratic_resampling = true;
      printf("[sdcard_startup] quadratic resampling\n");
    }

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
}

void update_fx(uint8_t fx_num) {
  switch (fx_num) {
    case FX_REVERSE:
      phase_forward = !sf->fx_active[fx_num];
      break;
    case FX_SATURATE:
      Saturation_setActive(saturation, sf->fx_active[fx_num]);
      break;
    case FX_BEATREPEAT:
      if (sf->fx_active[fx_num]) {
        BeatRepeat_repeat(beatrepeat,
                          sf->fx_param[FX_BEATREPEAT][0] * 19000 / 255 + 100);
      } else {
        BeatRepeat_repeat(beatrepeat, 0);
      }
      break;
    case FX_DELAY:
      Delay_setActive(delay, sf->fx_active[fx_num]);
      break;
    case FX_TIGHTEN:
      if (sf->fx_active[fx_num]) {
        Gate_set_amount(audio_gate, sf->fx_param[FX_TIGHTEN][0]);
      } else {
        Gate_set_amount(audio_gate, 255);
      }
      break;
    case FX_SLOWDOWN:
      if (sf->fx_active[fx_num]) {
        Envelope2_reset(envelope_pitch, BLOCKS_PER_SECOND,
                        Envelope2_update(envelope_pitch), 0.5, 1);
      } else {
        Envelope2_reset(envelope_pitch, BLOCKS_PER_SECOND,
                        Envelope2_update(envelope_pitch), 1.0, 1);
      }
      break;
    case FX_SPEEDUP:
      if (sf->fx_active[fx_num]) {
        Envelope2_reset(envelope_pitch, BLOCKS_PER_SECOND,
                        Envelope2_update(envelope_pitch), 2.0, 1);
      } else {
        Envelope2_reset(envelope_pitch, BLOCKS_PER_SECOND,
                        Envelope2_update(envelope_pitch), 1.0, 1);
      }
      break;
    case FX_TAPE_STOP:
      if (sf->fx_active[FX_TAPE_STOP]) {
        Envelope2_reset(envelope_pitch, BLOCKS_PER_SECOND,
                        Envelope2_update(envelope_pitch),
                        ENVELOPE_PITCH_THRESHOLD / 2, 2.7);
      } else {
        Envelope2_reset(envelope_pitch, BLOCKS_PER_SECOND,
                        Envelope2_update(envelope_pitch), 1.0, 1.9);
      }
      break;
    case FX_FUZZ:
      if (sf->fx_active[FX_FUZZ]) {
        printf("fuzz activated!\n");
      }
    case FX_FILTER:
      if (sf->fx_active[FX_FILTER]) {
        EnvelopeLinearInteger_reset(
            envelope_filter, BLOCKS_PER_SECOND,
            EnvelopeLinearInteger_update(envelope_filter, NULL), 5, 1.618);
      } else {
        EnvelopeLinearInteger_reset(
            envelope_filter, BLOCKS_PER_SECOND,
            EnvelopeLinearInteger_update(envelope_filter, NULL),
            global_filter_index, 1.618);
      }
      break;
    case FX_VOLUME_RAMP:
      if (sf->fx_active[FX_VOLUME_RAMP]) {
        Envelope2_reset(envelope_volume, BLOCKS_PER_SECOND,
                        Envelope2_update(envelope_volume), 0, 1.618 / 2);
      } else {
        Envelope2_reset(envelope_volume, BLOCKS_PER_SECOND,
                        Envelope2_update(envelope_volume), 1, 1.618 / 2);
      }
      break;
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
      fil_current_change = true;
      break;
    default:
      break;
  }
}

void fx_sequencer_emit(uint8_t key) {
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
    SaveFile_load(sf, &sync_using_sdcard, savefile_current);
    printf("[button_handler] loading %s again\n", fil_current_name);
    f_open(&fil_current, fil_current_name, FA_READ);
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
  while (!run_mount()) {
    sleep_ms(200);
  }
  envelope_volume = Envelope2_create(BLOCKS_PER_SECOND, 0, 1, 2);
  envelope_pitch = Envelope2_create(BLOCKS_PER_SECOND, 0.5, 1.0, 1.5);
  envelope_filter = EnvelopeLinearInteger_create(BLOCKS_PER_SECOND, 1,
                                                 resonantfilter_fc_max, 0.3);
  noise_wobble = Noise_create(time_us_64(), BLOCKS_PER_SECOND);
  audio_gate = Gate_create(BLOCKS_PER_SECOND, 165);
#ifdef INCLUDE_BASS
  bass = Bass_create();
#endif

  // sleep_ms(2000);
  check_setup_files();
  sleep_ms(2000);

  for (uint8_t bi = 0; bi < 16; bi++) {
    // TODO: show which banks are loading?
    // #ifdef INCLUDE_ZEPTOCORE
    //     for (uint8_t i = 0; i < bi; i++) {
    //       LEDS_set(leds, i, 2);
    //     }
    //     LEDS_render(leds);
    // #endif
    char dirname[10];
    sprintf(dirname, "bank%d\0", bi);
    banks[bi] = list_files(dirname);
    if (banks[bi]->num_samples > 0) {
      printf("[sdcard_startup] bank %d has %d samples\n", bi,
             banks[bi]->num_samples);
      banks_with_samples[banks_with_samples_num] = bi;
      banks_with_samples_num++;
      for (uint8_t si = 0; si < banks[bi]->num_samples; si++) {
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

  // sleep_ms(1000);

  uint32_t total_heap = getTotalHeap();
  uint32_t used_heap = total_heap - getFreeHeap();
  printf("memory usage: %2.1f%% (%ld/%ld)\n",
         (float)(used_heap) / (float)(total_heap)*100.0, used_heap, total_heap);

  FRESULT fr;
  sprintf(fil_current_name, "bank%d/%d.%d.wav", sel_bank_cur, sel_sample_cur,
          sel_variation);

  fr = f_open(&fil_current, fil_current_name, FA_READ);
  if (fr != FR_OK) {
    printf("[sdcard_startup] could not open %s: %s\n", fil_current_name,
           FRESULT_str(fr));
  }
  sf->vol = 180;
  phase_new = 0;
  phase_change = true;
  sync_using_sdcard = false;
  sdcard_startup_is_starting = false;

  savefile_do_load();

  fil_is_open = true;
  time_of_initialization = time_us_64();
}