
void check_setup_files() {
  DIR dj;      /* Directory object */
  FILINFO fno; /* File information */
  FRESULT fr;  /* File result error */

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
    fr = f_findnext(&dj, &fno); /* Search for next item */
  }
  f_closedir(&dj);
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

  check_setup_files();

  // sleep_ms(2000);

  for (uint8_t bi = 0; bi < 16; bi++) {
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
          printf("[sdcard_startup] printing information\n");
          printf(
              "[sdcard_startup] "
              "banks[%d]->sample[%d].snd[sel_variation]->size: %d\n",
              bi, si, banks[bi]->sample[si].snd[sel_variation]->size);
          printf(
              "[sdcard_startup] "
              "banks[%d]->sample[%d].snd[sel_variation]->num_channels: %d\n",
              bi, si, banks[bi]->sample[si].snd[sel_variation]->num_channels);
          printf(
              "[sdcard_startup] "
              "banks[%d]->sample[%d].snd[sel_variation]->oversampling: %d\n",
              bi, si, banks[bi]->sample[si].snd[sel_variation]->oversampling);
          printf(
              "[sdcard_startup] "
              "banks[%d]->sample[%d].snd[sel_variation]->splice_trigger:% d\n ",
              bi, si, banks[bi]->sample[si].snd[sel_variation]->splice_trigger);
          printf(
              "[sdcard_startup] "
              "banks[%d]->sample[%d].snd[sel_variation]->play_mode: "
              "% d\n ",
              bi, si, banks[bi]->sample[si].snd[sel_variation]->play_mode);
          printf(
              "[sdcard_startup] "
              "banks[%d]->sample[%d].snd[sel_variation]->bpm: "
              "%d\n",
              bi, si, banks[bi]->sample[si].snd[sel_variation]->bpm);
          printf(
              "[sdcard_startup] "
              "banks[%d]->sample[%d].snd[sel_variation]->slice_num: "
              "% d\n ",
              bi, si, banks[bi]->sample[si].snd[sel_variation]->slice_num);
          printf("slices: \n");
          for (uint8_t i = 0;
               i < banks[bi]->sample[si].snd[sel_variation]->slice_num; i++) {
            printf("%d) %d-%d\n", i,
                   banks[bi]->sample[si].snd[sel_variation]->slice_start[i],
                   banks[bi]->sample[si].snd[sel_variation]->slice_stop[i]);
          }
          printf("\n");
        }
      }
    }
  }  // bank loop

  uint32_t total_heap = getTotalHeap();
  uint32_t used_heap = total_heap - getFreeHeap();
  printf("memory usage: %2.1f%% (%ld/%ld)\n",
         (float)(used_heap) / (float)(total_heap)*100.0, used_heap, total_heap);

  FRESULT fr;
  char fname[100];
  sprintf(fname, "bank%d/%d.%d.wav", sel_bank_cur, sel_sample_cur,
          sel_variation);
  fr = f_open(&fil_current, fname, FA_READ);
  if (fr != FR_OK) {
    printf("[sdcard_startup] could not open %s: %s\n", fname, FRESULT_str(fr));
  }
  phase_new = 0;
  phase_change = true;
  sync_using_sdcard = false;
  sdcard_startup_is_starting = false;
  fil_is_open = true;
  time_of_initialization = time_us_64();
}