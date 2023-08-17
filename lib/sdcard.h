static sd_card_t *sd_get_by_name(const char *const name) {
  for (size_t i = 0; i < sd_get_num(); ++i)
    if (0 == strcmp(sd_get_by_num(i)->pcName, name)) return sd_get_by_num(i);
  DBG_PRINTF("%s: unknown name %s\n", __func__, name);
  return NULL;
}
static FATFS *sd_get_fs_by_name(const char *name) {
  for (size_t i = 0; i < sd_get_num(); ++i)
    if (0 == strcmp(sd_get_by_num(i)->pcName, name))
      return &sd_get_by_num(i)->fatfs;
  DBG_PRINTF("%s: unknown name %s\n", __func__, name);
  return NULL;
}

static void run_mount() {
  const char *arg1 = strtok(NULL, " ");
  arg1 = sd_get_by_num(0)->pcName;
  FATFS *p_fs = sd_get_fs_by_name(arg1);
  if (!p_fs) {
    printf("Unknown logical drive number: \"%s\"\n", arg1);
    return;
  }
  FRESULT fr = f_mount(p_fs, arg1, 1);
  if (FR_OK != fr) {
    printf("f_mount error: %s (%d)\n", FRESULT_str(fr), fr);
    return;
  }
  sd_card_t *pSD = sd_get_by_name(arg1);
  myASSERT(pSD);
  pSD->mounted = true;
}

static void run_cat() {
  char *arg1 = strtok(NULL, " ");
  if (!arg1) {
    arg1 = "2.raw";
  }
  FIL fil;
  FRESULT fr = f_open(&fil, arg1, FA_READ);
  if (FR_OK != fr) {
    printf("f_open error: %s (%d)\n", FRESULT_str(fr), fr);
    return;
  }
  char buf[256];
  unsigned int bytes_read;
  unsigned int pos;
  f_lseek(&fil, 1);
  f_read(&fil, buf, 256, &bytes_read);
  printf("bytes_read: %d\n");
  pos += bytes_read;
  printf("run_cat read %d bytes\n", bytes_read);
  fr = f_close(&fil);
  if (FR_OK != fr) printf("f_open error: %s (%d)\n", FRESULT_str(fr), fr);
}