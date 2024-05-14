
// toggle the fx
void toggle_fx(uint8_t fx_num) {
  sf->fx_active[fx_num] = !sf->fx_active[fx_num];
  update_fx(fx_num);
}

void toggle_fx_on(uint8_t fx_num, bool on) {
  sf->fx_active[fx_num] = on;
  update_fx(fx_num);
}

const bool pattern_reverse[4][16] = {
    {0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1},
    {0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1},
    {0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1},
    {0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1},
};

const bool pattern_fuzz[4][16] = {
    {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 1},
    {0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1},
    {0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1},
    {0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1},
};

const bool pattern_delay[4][16] = {
    {0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1},
    {0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1},
    {0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1},
    {0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1},
};

void update_patterns() {
  if ((pattern_reverse[0][beat_total % 16] && !sf->fx_active[FX_REVERSE]) ||
      (!pattern_reverse[0][beat_total % 16] && sf->fx_active[FX_REVERSE])) {
    toggle_fx(FX_REVERSE);
    printf("reverse: %d\n", sf->fx_active[FX_REVERSE]);
  }
  if ((pattern_fuzz[0][beat_total % 16] && !sf->fx_active[FX_FUZZ]) ||
      (!pattern_fuzz[0][beat_total % 16] && sf->fx_active[FX_FUZZ])) {
    toggle_fx(FX_FUZZ);
    printf("distortion: %d\n", sf->fx_active[FX_FUZZ]);
  }
  if ((pattern_delay[0][beat_total % 16] && !sf->fx_active[FX_DELAY]) ||
      (!pattern_delay[0][beat_total % 16] && sf->fx_active[FX_DELAY])) {
    toggle_fx(FX_DELAY);
    printf("delay: %d\n", sf->fx_active[FX_DELAY]);
  }
}