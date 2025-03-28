// Copyright 2023-2025 Zack Scholl, GPLv3.0

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../beatrepeat.h"

void write_wav_header(FILE *file, int num_samples) {
  int bits_per_sample = 16;
  int sample_rate = 44100;
  int byte_rate = sample_rate * (bits_per_sample / 8);
  int block_align = bits_per_sample / 8;
  int subchunk2_size = num_samples * block_align;
  int chunk_size = 36 + subchunk2_size;

  fwrite("RIFF", 1, 4, file);
  fwrite(&chunk_size, 4, 1, file);
  fwrite("WAVE", 1, 4, file);
  fwrite("fmt ", 1, 4, file);

  int subchunk1_size = 16;
  int16_t audio_format = 1;
  int16_t num_channels = 1;

  fwrite(&subchunk1_size, 4, 1, file);
  fwrite(&audio_format, 2, 1, file);
  fwrite(&num_channels, 2, 1, file);
  fwrite(&sample_rate, 4, 1, file);
  fwrite(&byte_rate, 4, 1, file);
  fwrite(&block_align, 2, 1, file);
  fwrite(&bits_per_sample, 2, 1, file);

  fwrite("data", 1, 4, file);
  fwrite(&subchunk2_size, 4, 1, file);
}
// WAV header structure
typedef struct {
  char riff_header[4];  // Contains "RIFF"
  int wav_size;         // Size of the WAV file
  char wave_header[4];  // Contains "WAVE"
  char fmt_header[4];   // Contains "fmt "
  int fmt_chunk_size;   // Should be 16 for PCM
  short audio_format;   // Should be 1 for PCM
  short num_channels;   // Number of channels
  int sample_rate;      // Sample rate
  int byte_rate;        // Bytes per second
  short sample_alignment;
  short bit_depth;      // Bits per sample
  char data_header[4];  // Contains "data"
  int data_bytes;       // Number of bytes in data
} wav_header;

int main() {
  FILE *file;
  wav_header header;
  int16_t *audio_data;
  int num_samples;

  // Open WAV file
  file = fopen("test.wav", "rb");
  if (!file) {
    perror("Error opening file");
    return 1;
  }

  // Read the header
  fread(&header, sizeof(wav_header), 1, file);

  // Check if the file is stereo and 16-bit PCM
  if (header.num_channels != 2 || header.bit_depth != 16) {
    fprintf(stderr,
            "Unsupported audio format (only stereo 16-bit PCM is supported)\n");
    fclose(file);
    return 1;
  }

  // Calculate the number of samples
  num_samples =
      header.data_bytes / (header.bit_depth / 8) / header.num_channels;
  audio_data = (int16_t *)malloc(num_samples * sizeof(int16_t));

  if (!audio_data) {
    perror("Memory allocation failed");
    fclose(file);
    return 1;
  }

  // Read and process audio data
  for (int i = 0; i < num_samples; i++) {
    short sample[2];
    fread(sample, sizeof(short), 2, file);  // Read one sample for each channel
    audio_data[i] = sample[0];              // Store the left channel
  }
  // Close file and free memory
  fclose(file);

  /* process the audio here*/
  BeatRepeat *br = BeatRepeat_malloc();
  for (int i = 0; i < num_samples; i += 1000) {
    int16_t audio_block[1000];
    memcpy(audio_block, audio_data + i, 1000 * sizeof(int16_t));
    if (i == 20000) {
      BeatRepeat_repeat(br, 15000);
    }
    if (i == 57000) {
      BeatRepeat_repeat(br, 0);
    }
    if (i == 60000) {
      BeatRepeat_repeat(br, 5000);
    }
    if (i == 80000) {
      BeatRepeat_repeat(br, 0);
    }
    if (i == 81000) {
      BeatRepeat_repeat(br, 1000);
    }
    if (i == 108000) {
      BeatRepeat_repeat(br, 0);
    }
    if (i == 155000) {
      BeatRepeat_repeat(br, 270);
    }
    if (i == 175000) {
      BeatRepeat_repeat(br, 0);
    }
    BeatRepeat_process(br, audio_block, 1000);
    memcpy(audio_data + i, audio_block, 1000 * sizeof(int16_t));
  }
  BeatRepeat_free(br);

  /* end processing the audio here */

  for (int i = 0; i < num_samples; i++) {
    printf("%d\n", audio_data[i]);
  }

  // Write WAV file
  FILE *file2 = fopen("test2.wav", "wb");
  if (!file2) {
    printf("Error opening file\n");
    return 1;
  }

  write_wav_header(file2, num_samples);
  fwrite(audio_data, sizeof(int16_t), num_samples, file2);

  fclose(file2);

  free(audio_data);

  return 0;
}

// WaveBass *osc = WaveBass_malloc();
// WaveBass_note_on(osc, 12);
// int16_t vals[8000];
// for (int i = 0; i < 8000; i++) {
//   vals[i] = WaveBass_next(osc) >> 16;
//   printf("%d\n", vals[i]);
// }
// BeatRepeat_process(br, vals, 8000);
// for (int i = 0; i < 8000; i++) {
//   printf("%d\n", vals[i]);
// }
// BeatRepeat_repeat(br, 200);
// for (int i = 0; i < 8000; i++) {
//   vals[i] = WaveBass_next(osc);
// }

// WaveBass_free(osc);
