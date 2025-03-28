#ifndef WRITESTRUCTTOFILE_H
#define WRITESTRUCTTOFILE_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct SampleInfo
{
  uint32_t bpm : 9;
  uint32_t beats : 9;
  uint32_t slice_num : 7;
  uint32_t slice_current : 7;
  uint8_t play_mode : 3;
  uint8_t splice_trigger : 2;
  uint8_t tempo_match : 1;
  uint8_t oversampling : 1;
  uint8_t num_channels : 1;
  int32_t *slice_start;
  int32_t *slice_stop;
} SampleInfo;

void SampleInfo_free(SampleInfo *si)
{
  if (si != NULL)
  {
    free(si->slice_start);
    free(si->slice_stop);
  }
  free(si);
}

SampleInfo *SampleInfo_malloc(uint32_t bpm, uint32_t beats, uint8_t play_mode,
                              uint8_t splice_trigger, uint8_t tempo_match,
                              uint8_t oversampling, uint8_t num_channels,
                              uint32_t slice_num, int32_t *slice_start,
                              int32_t *slice_stop)
{
  SampleInfo *si = (SampleInfo *)malloc(sizeof(SampleInfo));
  if (si == NULL)
  {
    perror("Error allocating memory for struct");
    return NULL;
  }
  // fprintf(stderr, "sizeof(SampleInfo) = %lu\n", sizeof(SampleInfo));

  si->bpm = bpm;
  si->beats = beats;
  si->slice_num = slice_num;
  si->slice_current = 0;
  si->play_mode = play_mode;
  si->splice_trigger = splice_trigger;
  si->tempo_match = tempo_match;
  si->oversampling = oversampling;
  si->num_channels = num_channels;

  si->slice_start = malloc(sizeof(int32_t) * si->slice_num);
  if (si->slice_start == NULL)
  {
    perror("Error allocating memory for array");
    free(si);
    return NULL;
  }
  for (int i = 0; i < si->slice_num; i++)
  {
    si->slice_start[i] = slice_start[i];
  }

  si->slice_stop = malloc(sizeof(int32_t) * si->slice_num);
  if (si->slice_stop == NULL)
  {
    perror("Error allocating memory for array");
    free(si->slice_start);
    free(si);
    return NULL;
  }
  for (int i = 0; i < si->slice_num; i++)
  {
    si->slice_stop[i] = slice_stop[i];
  }

  return si;
}

uint16_t SampleInfo_getBPM(SampleInfo *si) { return si->bpm; }

uint16_t SampleInfo_getSliceNum(SampleInfo *si) { return si->slice_num; }

int32_t SampleInfo_getSliceStop(SampleInfo *si, uint16_t i)
{
  return si->slice_stop[i];
}

int32_t SampleInfo_getSliceStart(SampleInfo *si, uint16_t i)
{
  return si->slice_start[i];
}

int SampleInfo_writeToDisk(SampleInfo *si)
{
  FILE *file = fopen("test.bin", "wb");
  if (file == NULL)
  {
    perror("Error opening file");
    SampleInfo_free(si);
    return -1;
  }

  // Write the struct (excluding the array)
  if (fwrite(si, sizeof(SampleInfo) - (2 * sizeof(int32_t *)), 1, file) != 1)
  {
    perror("Error writing struct to file");
    fclose(file);
    SampleInfo_free(si);
    return -1;
  }

  // Write the array content
  if (fwrite(si->slice_start, sizeof(int32_t), si->slice_num, file) !=
      si->slice_num)
  {
    perror("Error writing array to file");
    fclose(file);
    SampleInfo_free(si);
    return -1;
  }
  // Write the array content
  if (fwrite(si->slice_stop, sizeof(int32_t), si->slice_num, file) !=
      si->slice_num)
  {
    perror("Error writing array to file");
    fclose(file);
    SampleInfo_free(si);
    return -1;
  }

  fclose(file);
  return 0;
}

SampleInfo *SampleInfo_readFromDisk()
{
  FILE *file = fopen("test.bin", "rb");
  if (file == NULL)
  {
    perror("Error opening file");
    return NULL;
  }

  SampleInfo *si = (SampleInfo *)malloc(sizeof(SampleInfo));
  if (si == NULL)
  {
    perror("Error allocating memory");
    fclose(file);
    return NULL;
  }

  if (fread(si, sizeof(SampleInfo) - (2 * sizeof(int32_t *)), 1, file) != 1)
  {
    perror("Error reading struct from file");
    fclose(file);
    SampleInfo_free(si);
    return NULL;
  }

  // Allocate memory for the array
  si->slice_start = (int32_t *)malloc(sizeof(int32_t) * si->slice_num);
  if (si->slice_start == NULL)
  {
    perror("Error allocating memory for array");
    fclose(file);
    SampleInfo_free(si);
    return NULL;
  }

  // Read the array content
  if (fread(si->slice_start, sizeof(int32_t), si->slice_num, file) !=
      si->slice_num)
  {
    perror("Error reading array from file");
    fclose(file);
    SampleInfo_free(si);
    return NULL;
  }

  // Allocate memory for the slice_stop array
  si->slice_stop = (int32_t *)malloc(sizeof(int32_t) * si->slice_num);
  if (si->slice_stop == NULL)
  {
    perror("Error allocating memory for slice_stop array");
    fclose(file);
    free(si->slice_start);
    SampleInfo_free(si);
    return NULL;
  }

  // Read the slice_stop array content
  if (fread(si->slice_stop, sizeof(int32_t), si->slice_num, file) !=
      si->slice_num)
  {
    perror("Error reading slice_stop array from file");
    fclose(file);
    free(si->slice_start);
    free(si->slice_stop);
    SampleInfo_free(si);
    return NULL;
  }

  fclose(file);
  return si;
}

#endif // WRITESTRUCTTOFILE_H
