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
#include "wav.h"

#define FILELISTNAME "filelist120"

unsigned int extract_bpm(const char *input) {
  int len = strlen(input);
  int i = 0;
  bool foundBPM = false;

  while (i < len) {
    // Check for the pattern "bpm"
    if (input[i] == 'b' && input[i + 1] == 'p' && input[i + 2] == 'm') {
      foundBPM = true;
      i += 3;  // Skip "bpm"
      break;
    }
    i++;
  }

  // If "bpm" is found, extract the number
  if (foundBPM) {
    char numberBuffer[10];
    int j = 0;

    while (i < len && isdigit(input[i])) {
      numberBuffer[j] = input[i];
      i++;
      j++;
    }

    numberBuffer[j] = '\0';
    int bpmValue = atoi(numberBuffer);

    return bpmValue;
  } else {
    return 1;  // If "bpm" is not found, return 1 to indicate failure
  }
}

unsigned int extract_beats(const char *input) {
  int len = strlen(input);
  int i = 0;
  bool foundBeats = false;

  while (i < len) {
    // Check for the pattern "bpm"
    if (input[i] == 'b' && input[i + 1] == 'e' && input[i + 2] == 'a' &&
        input[i + 3] == 't' && input[i + 4] == 's') {
      foundBeats = true;
      i += 5;  // Skip "beats"
      break;
    }
    i++;
  }

  // If "bpm" is found, extract the number
  if (foundBeats) {
    char numberBuffer[10];
    int j = 0;

    while (i < len && isdigit(input[i])) {
      numberBuffer[j] = input[i];
      i++;
      j++;
    }

    numberBuffer[j] = '\0';
    int bpmValue = atoi(numberBuffer);

    return bpmValue;
  } else {
    return 1;  // If "bpm" is not found, return 1 to indicate failure
  }
}

#define WAV_HEADER_SIZE 44
#define FileList_max 16
typedef struct FileList {
  char name[FileList_max][100];
  uint32_t size[FileList_max];
  unsigned int bpm[FileList_max];
  unsigned int beats[FileList_max];
  unsigned int num;
} FileList;

FileList list_files(const char *dir, int num_channels) {
  FileList filelist;
  for (uint8_t i = 0; i < FileList_max; i++) {
    filelist.size[i] = 0;
    filelist.bpm[i] = 0;
    filelist.beats[i] = 0;
  }
  filelist.num = 0;

  char cwdbuf[FF_LFN_BUF] = {0};
  FRESULT fr; /* Return value */
  char const *p_dir;
  if (dir[0]) {
    p_dir = dir;
  } else {
    fr = f_getcwd(cwdbuf, sizeof cwdbuf);
    if (FR_OK != fr) {
      printf("f_getcwd error: %s (%d)\n", FRESULT_str(fr), fr);
      return filelist;
    }
    p_dir = cwdbuf;
  }

  char filelist_name[100];
  strcpy(filelist_name, p_dir);
  strcat(filelist_name, "/");
  strcat(filelist_name, FILELISTNAME);

  FIL fil; /* File object */
  if (f_open(&fil, filelist_name, FA_READ)) {
    {
      printf("[file_list] file list '%s' does not exist\n", filelist_name);
      // creating new file list
      DIR dj;      /* Directory object */
      FILINFO fno; /* File information */
      memset(&dj, 0, sizeof dj);
      memset(&fno, 0, sizeof fno);
      fr = f_findfirst(&dj, &fno, p_dir, "*");
      if (FR_OK != fr) {
        printf("[file_list] f_findfirst error: %s (%d)\n", FRESULT_str(fr), fr);
        return filelist;
      }
      while (fr == FR_OK && fno.fname[0]) { /* Repeat while an item is found */
        /* Create a string that includes the file name, the file size and the
         attributes string. */
        const char *pcWritableFile = "writable file",
                   *pcReadOnlyFile = "read only file",
                   *pcDirectory = "directory";
        const char *pcAttrib;
        /* Point pcAttrib to a string that describes the file. */
        if (fno.fattrib & AM_DIR) {
          pcAttrib = pcDirectory;
        } else if (fno.fattrib & AM_RDO) {
          pcAttrib = pcReadOnlyFile;
        } else {
          pcAttrib = pcWritableFile;
        }
        /* Create a string that includes the file name, the file size and the
         attributes string. */
        // printf("%s [%s] [size=%llu]\n", fno.fname, pcAttrib, fno.fsize);

        // Check if the filename ends with ".wav"
        if (strstr(fno.fname, ".wav") && strstr(fno.fname, "bpm") &&
            strstr(fno.fname, "beats")) {
          filelist.bpm[filelist.num] = extract_bpm(fno.fname);
          filelist.beats[filelist.num] = extract_beats(fno.fname);
          printf("%s, beats=%d, bpm=%d\n", fno.fname,
                 filelist.beats[filelist.num], filelist.bpm[filelist.num]);
          if (filelist.bpm[filelist.num] > 10 &&
              filelist.beats[filelist.num] > 1) {
            WavHeader *wh;
            char full_fname[100];
            strcpy(full_fname, p_dir);
            strcat(full_fname, "/");
            strcat(full_fname, fno.fname);
            wh = WavFile_Load(full_fname);
            if (wh->NumOfChan == num_channels) {
              // Allocate memory for the name field and copy the filename into
              // it
              strcpy(filelist.name[filelist.num], full_fname);
              sprintf(filelist.name[filelist.num], "%s", full_fname);
              filelist.size[filelist.num] =
                  (uint32_t)(fno.fsize - WAV_HEADER_SIZE);
              filelist.num++;
            }
            free(wh);
          }
        }
        // limit to 16 files!
        if (filelist.num == 16) {
          break;
        }

        fr = f_findnext(&dj, &fno); /* Search for next item */
      }
      f_closedir(&dj);
    }
    {
      printf("[file_list] writing %s\n", filelist_name);
      FRESULT fr;
      FIL file; /* File object */
      fr = f_open(&file, filelist_name, FA_WRITE | FA_CREATE_ALWAYS);
      if (FR_OK != fr) {
        printf("[file_list] f_open error: %s (%d)\n", FRESULT_str(fr), fr);
      } else {
        unsigned int bw;
        fr = f_write(&file, &filelist, sizeof(FileList), &bw);
        if (fr != FR_OK) {
          printf("[file_list] problem writing save: %s\n", FRESULT_str(fr));
        }
        printf("[file_list] wrote %d bytes\n", bw);
        fr = f_close(&file);
        if (fr != FR_OK) {
          printf("[file_list] problem closing file: %s\n", FRESULT_str(fr));
        }
      }
    }
  } else {
    unsigned int bytes_read;
    if (f_read(&fil, &filelist, sizeof(FileList), &bytes_read)) {
      printf("[file_list] problem reading save file");
    }
    printf("[file_list] bytes read: %d\n", bytes_read);
    f_close(&fil);
  }

  return filelist;
}
