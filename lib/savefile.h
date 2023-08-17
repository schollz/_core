
typedef struct SaveFile {
  uint8_t chain_length;
  uint8_t chain_sequence[255];
  uint8_t pattern_length[16];
  uint8_t pattern_sequence[16][255];
  uint16_t bpm_tempo;
} SaveFile;

#define SAVEFILE_PATHNAME "save.bin"

SaveFile *SaveFile_Load(FIL *fil) {
  printf("[SaveFile] reading\n");
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
  if (f_open(fil, SAVEFILE_PATHNAME, FA_READ)) {
    printf("[SaveFile] no save file, skipping ");
  } else {
    unsigned int bytes_read;
    if (f_read(fil, sf, sizeof(SaveFile), &bytes_read)) {
      printf("[SaveFile] problem reading save file");
    } else {
      printf("[SaveFile] bpm_tempo = %d\n", sf->bpm_tempo);
    }
  }

  f_close(fil);
  return sf;
}

#define PRE_ALLOCATE false

bool SaveFile_Save(SaveFile *sf) {
  printf("[SaveFile] writing\n");

  /* Open the file, creating the file if it does not already exist. */
  FRESULT fr;
  FIL file; /* File object */
  FILINFO fno;
  size_t fsz = 0;
  fr = f_stat(SAVEFILE_PATHNAME, &fno);
  if (FR_OK == fr) fsz = fno.fsize;
  if (0 < fsz && fsz <= sizeof(SaveFile)) {
    // This is an attempt at optimization:
    // rewriting the file should be faster than
    // writing it from scratch.
    fr = f_open(&file, SAVEFILE_PATHNAME, FA_READ | FA_WRITE);
    if (FR_OK != fr) {
      printf("f_open error: %s (%d)\n", FRESULT_str(fr), fr);
      return false;
    }
    fr = f_rewind(&file);
    if (FR_OK != fr) {
      printf("f_rewind error: %s (%d)\n", FRESULT_str(fr), fr);
      return false;
    }
  } else {
    fr = f_open(&file, SAVEFILE_PATHNAME, FA_WRITE | FA_CREATE_ALWAYS);
    if (FR_OK != fr) {
      printf("f_open error: %s (%d)\n", FRESULT_str(fr), fr);
      return false;
    }
  }
  if (PRE_ALLOCATE) {
    FRESULT fr = f_lseek(&file, sizeof(SaveFile));
    if (FR_OK != fr) {
      printf("f_lseek error: %s (%d)\n", FRESULT_str(fr), fr);
      return false;
    }
    if (f_tell(&file) != sizeof(SaveFile)) {
      printf("Disk full?\n");
      return false;
    }
    fr = f_rewind(&file);
    if (FR_OK != fr) {
      printf("f_rewind error: %s (%d)\n", FRESULT_str(fr), fr);
      return false;
    }
  }
  unsigned int bw;
  if (f_write(&file, sf, sizeof(SaveFile), &bw)) {
    printf("[SaveFile] problem writing save\n");
  } else {
    if (bw < sizeof(SaveFile)) {
      printf("f_write(%s,,%d,): only wrote %d bytes\n", SAVEFILE_PATHNAME,
             sizeof(SaveFile), bw);
      return false;
    }
  }

  f_close(&file);
}
