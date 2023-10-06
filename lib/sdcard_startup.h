
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

  printf("\nz!!\n");

  for (uint8_t i = 0; i < 1; i++) {
    char dirname[10];
    sprintf(dirname, "bank%d\0", i);
    file_list[i] = list_files(dirname, WAV_CHANNELS);
    printf("bank %d, ", i);
    printf("found %d files\n", file_list[i].num);
    printf("file_list[i].name[0]: %s\n", file_list[i].name[0]);
    printf("file_list[i].bpm[0]: %d\n", file_list[i].bpm[0]);
    printf("file_list[i].beats[0]: %d\n", file_list[i].beats[0]);
    printf("file_list[i].size[0]: %d\n", file_list[i].size[0]);
    printf("file_list[i].name[0]: %s\n", file_list[i].name[1]);
    printf("file_list[i].bpm[0]: %d\n", file_list[i].bpm[1]);
    printf("file_list[i].beats[0]: %d\n", file_list[i].beats[1]);
    printf("file_list[i].size[0]: %d\n", file_list[i].size[1]);
  }

  fil_current_bank = 0;
  fil_current_bank_next = 0;
  fil_current_id = 0;
  fil_current_id_next = 0;
  FRESULT fr;
  fr = f_open(&fil_current, file_list[fil_current_bank].name[fil_current_id],
              FA_READ);
  if (fr != FR_OK) {
    printf("could not open: %s\n",
           file_list[fil_current_bank].name[fil_current_id]);
  }
  phase_new = 0;
  phase_change = true;
  sync_using_sdcard = false;
  sdcard_startup_is_starting = false;
  fil_is_open = true;
}