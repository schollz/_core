// Copyright 2023 Zack Scholl.
//
// Author: Zack Scholl (zack.scholl@gmail.com)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// See http://creativecommons.org/licenses/MIT/ for more information.

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
