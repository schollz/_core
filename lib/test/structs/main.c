// Copyright 2023-2024 Zack Scholl.
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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FileList_max 255
typedef struct FileList {
  char **name;
  char *dir;
  uint32_t *size;
  unsigned int *bpm;
  unsigned int num;
} FileList;

FileList *list_files(char *dir, int num_channels) {
  FileList *filelist = malloc(sizeof(FileList));
  filelist->dir = malloc(strlen(dir) + 1);
  strcpy(filelist->dir, dir);
  filelist->name = malloc(sizeof(char *) * FileList_max);
  filelist->size = malloc(sizeof(10) * FileList_max);
  filelist->bpm = malloc(sizeof(unsigned int) * FileList_max);
  filelist->num = 1;
  return filelist;
}

int main() {
  FileList *fh[3];
  for (short i = 0; i < 3; i++) {
    char dirname[10];
    sprintf(dirname, "bank%d\0", i + 1);
    fh[i] = list_files(dirname, 1);
    printf("fh->num: %d\n", fh[i]->num);
    printf("dirname: %s\n", dirname);
    printf("fh->dir: %s\n", fh[i]->dir);
  }

  return 0;
}