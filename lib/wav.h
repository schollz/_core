// Copyright 2023-2025 Zack Scholl, GPLv3.0

#ifndef WAVH
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

WavHeader *WavFile_Load(char *fname) {
  FIL fil;
  f_open(&fil, fname, FA_READ);
  int headerSize = sizeof(WavHeader);
  WavHeader *wh;
  wh = malloc(headerSize);
  unsigned int bytes_read;
  FRESULT fr = f_read(&fil, wh, headerSize, &bytes_read);
  if (fr != FR_OK || bytes_read != headerSize) {
    debugf("fname: %s", fname);
    debugf("bytes_read: %d", bytes_read);
    debugf("fr: %d", fr);
    return NULL;
  }
  // printf("file: %s\n", fname);
  // printf("NumOfChan: %d\n", wh->NumOfChan);
  // printf("SamplesPerSec: %d\n", wh->SamplesPerSec);
  // printf("RIFF: %d\n", wh->RIFF);
  // printf("ChunkSize: %d\n", wh->ChunkSize);
  // printf("WAVE: %d\n", wh->WAVE);
  // printf("fmt: %d\n", wh->fmt);
  // printf("Subchunk1Size: %d\n", wh->Subchunk1Size);
  // printf("AudioFormat: %d\n", wh->AudioFormat);
  // printf("bytesPerSec: %d\n", wh->bytesPerSec);
  // printf("blockAlign: %d\n", wh->blockAlign);
  // printf("bitsPerSample: %d\n", wh->bitsPerSample);
  // printf("Subchunk2ID: %d\n", wh->Subchunk2ID);
  // printf("Subchunk2Size: %d\n", wh->Subchunk2Size);

  f_close(&fil);
  return wh;
}
#define WAVH 1
#endif