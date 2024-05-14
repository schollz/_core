
// toggle the fx
void toggle_fx(uint8_t fx_num) {
  sf->fx_active[fx_num] = !sf->fx_active[fx_num];
  update_fx(fx_num);
}

void toggle_fx_on(uint8_t fx_num, bool on) {
  sf->fx_active[fx_num] = on;
  update_fx(fx_num);
}

#define PATTERN_REVERSE_SIZE 4
#define PATTERN_REVERSE_SLICES 32
const bool pattern_reverse[4][32] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1,
     0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1},
    {0, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 0, 1,
     0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1},
    {0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};

#define PATTERN_FILTER_SIZE 4
#define PATTERN_FILTER_SLICES 32
const bool pattern_filter[4][32] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1,
     0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1},
    {0, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 0, 1,
     0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1},
    {0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};

#define PATTERN_RETRIGGER_SIZE 4
#define PATTERN_RETRIGGER_SLICES 32
const uint8_t pattern_retrigger[4][32] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0},
    {0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};

#define PATTERN_DELAY_SIZE 5
#define PATTERN_DELAY_SLICES 47
const uint8_t pattern_delay[5][47] = {
    {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    },
    {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    },
    {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    },
    {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    },
    {
        0, 0, 0, 0, 0, 0, 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
        0, 0, 0, 0, 0, 0, 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
        0, 0, 0, 0, 0, 0, 120, 240, 230, 240, 220, 210, 200, 180, 140,
    },
};

uint8_t pattern_val = 0;

void update_pattern_val(uint8_t pattern_val_new) {
  pattern_val = pattern_val_new;
}
void update_patterns() {
  if ((pattern_reverse[pattern_val * PATTERN_REVERSE_SIZE / 255]
                      [beat_total % PATTERN_REVERSE_SLICES] &&
       !sf->fx_active[FX_REVERSE]) ||
      (!pattern_reverse[pattern_val * PATTERN_REVERSE_SIZE / 255]
                       [PATTERN_REVERSE_SLICES % 16] &&
       sf->fx_active[FX_REVERSE])) {
    toggle_fx(FX_REVERSE);
    printf("reverse (%d): %d\n", pattern_val * PATTERN_REVERSE_SIZE / 255,
           sf->fx_active[FX_REVERSE]);
  }

  if ((pattern_filter[pattern_val * PATTERN_FILTER_SIZE / 255]
                     [beat_total % PATTERN_FILTER_SLICES] &&
       !sf->fx_active[FX_FILTER]) ||
      (!pattern_filter[pattern_val * PATTERN_FILTER_SIZE / 255]
                      [beat_total % PATTERN_FILTER_SLICES] &&
       sf->fx_active[FX_FILTER])) {
    toggle_fx(FX_FILTER);
    printf("filter (%d): %d\n", pattern_val * PATTERN_FILTER_SIZE / 255,
           sf->fx_active[FX_FILTER]);
  }
  if ((pattern_delay[pattern_val * PATTERN_DELAY_SIZE / 255]
                    [beat_total % PATTERN_DELAY_SLICES] > 0 &&
       !sf->fx_active[FX_DELAY]) ||
      (pattern_delay[pattern_val * PATTERN_DELAY_SIZE / 255]
                    [beat_total % PATTERN_DELAY_SLICES] == 0 &&
       sf->fx_active[FX_DELAY])) {
    if (pattern_delay[pattern_val * PATTERN_DELAY_SIZE / 255]
                     [beat_total % PATTERN_DELAY_SLICES] > 0) {
      sf->fx_param[FX_DELAY][0] =
          pattern_delay[pattern_val * PATTERN_DELAY_SIZE / 255]
                       [beat_total % PATTERN_DELAY_SLICES];
    }
    toggle_fx(FX_DELAY);
    printf("delay (%d): %d\n", pattern_val * PATTERN_DELAY_SIZE / 255,
           sf->fx_active[FX_DELAY]);
  }

  if (pattern_retrigger[pattern_val * PATTERN_RETRIGGER_SIZE / 255]
                       [beat_total % PATTERN_RETRIGGER_SLICES] > 0) {
    debounce_quantize = 0;
    retrig_first = true;
    retrig_beat_num =
        pattern_retrigger[pattern_val * PATTERN_RETRIGGER_SIZE / 255]
                         [beat_total % PATTERN_RETRIGGER_SLICES];
    retrig_timer_reset = 96 / 4;
    retrig_vol_step = 1.0 / ((float)retrig_beat_num);
    retrig_ready = true;

    printf("retrigger (%d): %d\n", pattern_val * PATTERN_RETRIGGER_SIZE / 255,
           retrig_beat_num);
  }
}