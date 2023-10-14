
void sdcard_startup() {
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
  envelope1 = Envelope2_create(BLOCKS_PER_SECOND, 0.01, 1, 1.5);
  envelope2 = Envelope2_create(BLOCKS_PER_SECOND, 1, 0, 0.01);
  envelope3 = Envelope2_create(BLOCKS_PER_SECOND, 0, 1, 2);
  envelope_pitch = Envelope2_create(BLOCKS_PER_SECOND, 0.5, 1.0, 1.5);
  envelopegate = EnvelopeGate_create(BLOCKS_PER_SECOND, 1, 1, 0.5, 0.5);
  noise_wobble = Noise_create(time_us_64(), BLOCKS_PER_SECOND);
#ifdef INCLUDE_BASS
  bass = Bass_create();
#endif

  for (uint8_t bi = 0; bi < 3; bi++) {
    char dirname[10];
    sprintf(dirname, "bank%d\0", bi);
    banks[bi] = list_files(dirname, 1);
    if (banks[bi]->num_samples == 0) {
      printf("[keyboard] banks[%d]: no samples\n", bi);
    } else {
      for (uint8_t si = 0; si < banks[bi]->num_samples; si++) {
        if (bi == 0 && si == 0) {
          printf("[keyboard] banks[%d]->sample[%d].snd[0]->name: %s\n", bi, si,
                 banks[bi]->sample[si].snd[0]->name);
          printf("[keyboard] banks[%d]->sample[%d].snd[0]->size: %d\n", bi, si,
                 banks[bi]->sample[si].snd[0]->size);
          printf("[keyboard] banks[%d]->sample[%d].snd[0]->bpm: %d\n", bi, si,
                 banks[bi]->sample[si].snd[0]->bpm);
          printf("[keyboard] banks[%d]->sample[%d].snd[0]->beats: %d\n", bi, si,
                 banks[bi]->sample[si].snd[0]->beats);
          printf("[keyboard] banks[%d]->sample[%d].snd[0]->slice_num: % d\n ",
                 bi, si, banks[bi]->sample[si].snd[0]->slice_num);
          printf("slices: \n");
          for (uint8_t i = 0; i < banks[bi]->sample[si].snd[0]->slice_num;
               i++) {
            printf("%d) %d-%d\n", i,
                   banks[bi]->sample[si].snd[0]->slice_start[i],
                   banks[bi]->sample[si].snd[0]->slice_stop[i]);
          }
          printf("\n");
        }
      }
    }
  }  // bank loop

  FRESULT fr;
  fr = f_open(&fil_current, file_list[sel_bank_cur].name[sel_sample_cur],
              FA_READ);
  if (fr != FR_OK) {
    printf("could not open: %s\n",
           file_list[sel_bank_cur].name[sel_sample_cur]);
  }
  phase_new = 0;
  phase_change = true;
  sync_using_sdcard = false;
  sdcard_startup_is_starting = false;
  fil_is_open = true;
}