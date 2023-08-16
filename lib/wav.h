
typedef struct WavHeader {
  uint8_t RIFF[4];
  uint32_t ChunkSize;
  uint8_t WAVE[4];
  uint8_t fmt[4];
  uint32_t Subchunk1Size;
  uint16_t AudioFormat;
  uint16_t NumOfChan;
  uint32_t SamplesPerSec;
  uint32_t bytesPerSec;
  uint16_t blockAlign;
  uint16_t bitsPerSample;
  uint8_t Subchunk2ID[4];
  uint32_t Subchunk2Size;
} WavHeader;

WavHeader *WavFile_Load(FIL *fil) {
  printf("reading wave header\n");
  int headerSize = sizeof(WavHeader);
  WavHeader *wh;
  wh = malloc(headerSize);
  printf("headerSize: %d\n", headerSize);
  unsigned int bytes_read;
  FRESULT fr = f_read(fil, wh, headerSize, &bytes_read);
  printf("bytes_read: %d\n");
  if (fr != FR_OK) {
    printf("fr = %d, not OK!\n", fr);
    return NULL;
  }
  // printf("RIFF: %d\n", wh->RIFF);
  printf("ChunkSize: %d\n", wh->ChunkSize);
  // printf("WAVE: %d\n", wh->WAVE);
  // printf("fmt: %d\n", wh->fmt);
  // printf("Subchunk1Size: %d\n", wh->Subchunk1Size);
  // printf("AudioFormat: %d\n", wh->AudioFormat);
  // printf("NumOfChan: %d\n", wh->NumOfChan);
  // printf("SamplesPerSec: %d\n", wh->SamplesPerSec);
  // printf("bytesPerSec: %d\n", wh->bytesPerSec);
  // printf("blockAlign: %d\n", wh->blockAlign);
  // printf("bitsPerSample: %d\n", wh->bitsPerSample);
  // printf("Subchunk2ID: %d\n", wh->Subchunk2ID);
  // printf("Subchunk2Size: %d\n", wh->Subchunk2Size);
  return wh;
}
