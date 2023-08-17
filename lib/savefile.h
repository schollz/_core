
typedef struct SaveFile {
  uint8_t chain_length;
  uint8_t chain_sequence[255];
  uint8_t pattern_length[16];
  uint8_t pattern_sequence[16][255];
  uint16_t bpm_tempo;
} SaveFile;

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
  if (f_open(fil, "save.bin", FA_READ)) {
    printf("[SaveFile] no save file, skipping ");
  } else {
    unsigned int bytes_read;
    if (f_read(fil, sf, sizeof(SaveFile), &bytes_read)) {
      printf("[SaveFile] problem reading save file");
    } else {
      printf("[SaveFile] bpm_tempo = %d\n", sf->bpm_tempo);
    }
  }
  return sf;
}
