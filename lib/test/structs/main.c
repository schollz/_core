// Copyright 2023-2025 Zack Scholl, GPLv3.0

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