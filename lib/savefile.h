
typedef struct SaveFile {
  uint8_t chain_length;
  uint8_t chain_sequence[255];
  uint8_t pattern_length[16];
  uint8_t pattern_sequence[16][255];
  uint16_t bpm_tempo;
} SaveFile;

#define SAVEFILE_PATHNAME "save.bin"

SaveFile *SaveFile_New() {
  SaveFile *sf;
  sf = malloc(sizeof(SaveFile));
  sf->bpm_tempo = 165;
  sf->chain_length = 0;
  for (uint8_t i = 0; i < 255; i++) {
    sf->chain_sequence[i] = 0;
  }
  for (uint8_t i = 0; i < 16; i++) {
    sf->pattern_length[i] = 0;
    for (uint8_t j = 0; j < 255; j++) {
      sf->pattern_sequence[i][j] = 0;
    }
  }
  return sf;
}

bool SaveFile_Load(SaveFile *sf) {
  FIL fil; /* File object */
  printf("[SaveFile] reading\n");
  if (f_open(&fil, SAVEFILE_PATHNAME, FA_READ)) {
    printf("[SaveFile] no save file, skipping ");
  } else {
    unsigned int bytes_read;
    if (f_read(&fil, sf, sizeof(SaveFile), &bytes_read)) {
      printf("[SaveFile] problem reading save file");
    } else {
      printf("[SaveFile] bpm_tempo = %d\n", sf->bpm_tempo);
    }
  }
  f_close(&fil);
  return true;
}

bool SaveFile_Save(SaveFile *sf, bool *sync_sd_card) {
  while (*sync_sd_card) {
    sleep_us(100);
  }
  *sync_sd_card = true;
  printf("[SaveFile] writing\n");
  FRESULT fr;
  FIL file; /* File object */
  printf("[SaveFile] opening savefile for writing\n");
  fr = f_open(&file, SAVEFILE_PATHNAME, FA_WRITE | FA_CREATE_ALWAYS);
  if (FR_OK != fr) {
    printf("f_open error: %s (%d)\n", FRESULT_str(fr), fr);
    *sync_sd_card = false;
    return false;
  }
  unsigned int bw;
  if (f_write(&file, sf, sizeof(SaveFile), &bw)) {
    printf("[SaveFile] problem writing save\n");
  }
  printf("[SaveFile] wrote %d bytes\n", bw);
  f_close(&file);
  *sync_sd_card = false;
  return true;
}
