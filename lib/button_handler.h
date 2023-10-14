#include "globals.h"

// keys
#define KEY_SHIFT 0
#define KEY_A 1
#define KEY_B 2
#define KEY_C 3
#define MODE_JUMP 0
#define MODE_MASH 1
#define MODE_SAMP 0
#define MODE_BANK 1

uint8_t key_held_num = 0;
bool key_held_on = false;
int32_t key_timer = 0;
uint8_t key_pressed[100];
uint8_t key_pressed_num = 0;
uint8_t key_total_pressed = 0;
int16_t key_on_buttons[BUTTONMATRIX_BUTTONS_MAX];
uint16_t key_num_presses;

// fx toggles
bool fx_toggle[16];  // 16 possible
#define FX_REVERSE 0
#define FX_SLOWDOWN 1
#define FX_NORMSPEED 2
#define FX_SPEEDUP 3

void key_do_jump(uint8_t beat) {
  if (beat >= 0 && beat < 16) {
    beat_current = (beat_current / 16) * 16 + beat;
    do_update_phase_from_beat_current();
    LEDS_clearAll(leds, LED_STEP_FACE);
    LEDS_set(leds, LED_STEP_FACE, beat_current % 16 + 4, 2);
  }
}

void go_retrigger_3key(uint8_t key1, uint8_t key2, uint8_t key3) {
  debounce_quantize = 0;
  retrig_first = true;
  retrig_beat_num = key2 + 4;
  retrig_timer_reset = 96 / key3;
  float total_time = (float)(retrig_beat_num * retrig_timer_reset * 60) /
                     (float)(96 * sf->bpm_tempo);
  retrig_vol_step = 1.0 / ((float)retrig_beat_num);
  printf("retrig_beat_num=%d,retrig_timer_reset=%d,total_time=%2.3fs\n",
         retrig_beat_num, retrig_timer_reset, total_time);
  retrig_ready = true;
}

void go_retrigger_2key(uint8_t key1, uint8_t key2) {
  debounce_quantize = 0;
  retrig_first = true;
  retrig_beat_num = random_integer_in_range(8, 24);
  retrig_timer_reset =
      96 * random_integer_in_range(1, 4) / random_integer_in_range(2, 12);
  float total_time = (float)(retrig_beat_num * retrig_timer_reset * 60) /
                     (float)(96 * sf->bpm_tempo);
  if (total_time > 2.0f) {
    total_time = total_time / 2;
    retrig_timer_reset = retrig_timer_reset / 2;
  }
  if (total_time > 2.0f) {
    total_time = total_time / 2;
    retrig_beat_num = retrig_beat_num / 2;
    if (retrig_beat_num == 0) {
      retrig_beat_num = 1;
    }
  }
  if (total_time < 0.25f) {
    total_time = total_time * 2;
    retrig_beat_num = retrig_beat_num * 2;
    if (retrig_beat_num == 0) {
      retrig_beat_num = 1;
    }
  }
  retrig_vol_step = 1.0 / ((float)retrig_beat_num);
  printf("retrig_beat_num=%d,retrig_timer_reset=%d,total_time=%2.3fs\n",
         retrig_beat_num, retrig_timer_reset, total_time);
  retrig_ready = true;
}

void go_update_top() {
  bool all_off = true;
  for (uint8_t i = 0; i < 4; i++) {
    if (key_on_buttons[i] > 0) {
      all_off = false;
    }
  }

  if (all_off) {
    // top row is not held
    // show jump/mash, mute, play
    LEDS_set(leds, KEY_A, 0, 2 * (1 - mode_jump_mash));
    LEDS_set(leds, KEY_B, 0, 2 * mode_mute);
    LEDS_set(leds, KEY_C, 0, 2 * mode_play);
  }
}

// uptate all the fx based on the fx_toggle
void go_update_fx(uint8_t fx_num) {
  bool on = fx_toggle[fx_num];
  switch (fx_num) {
    case FX_REVERSE:
      phase_forward = !on;
      break;
    case FX_SLOWDOWN:
      Envelope2_reset(envelope_pitch, BLOCKS_PER_SECOND,
                      Envelope2_update(envelope_pitch), 0.5, 1);
      break;
    case FX_NORMSPEED:
      Envelope2_reset(envelope_pitch, BLOCKS_PER_SECOND,
                      Envelope2_update(envelope_pitch), 1.0, 1);
      break;
    case FX_SPEEDUP:
      Envelope2_reset(envelope_pitch, BLOCKS_PER_SECOND,
                      Envelope2_update(envelope_pitch), 2.0, 1);
      break;
    default:
      break;
  }
}

void button_key_off_held(uint8_t key) {
  printf("off held %d\n", key);
  if (key == KEY_A) {
    // bank/sample toggler
    for (uint8_t i = 0; i < 4; i++) {
      LEDS_set(leds, 2, i, 0);
    }
  }
}

// triggers on ANY key off, used for 1-16 off's
void button_key_off_any(uint8_t key) {
  printf("off any %d\n", key);
  if (key > 3) {
    // 1-16 off
    if (mode_jump_mash == MODE_MASH) {
      // 1-16 off (mash mode)
      // remove momentary fx
      fx_toggle[key - 4] = false;
      go_update_fx(key - 4);
    }
  }
}

void button_key_on_single(uint8_t key) {
  printf("on %d\n", key);
  if (key < 4) {
    // TODO:
    // highlight toggle mode
    if (key == KEY_A) {
      if (mode_samp_bank) {
        // TODO: check this....
        LEDS_set(leds, 2, 2, 1);
        LEDS_set(leds, 2, 3, 0);
      };
    }
  } else {
    // 1-16
    if (mode_jump_mash == MODE_JUMP) {
      // 1-16 (jump mode)
      // do jump
      key_do_jump(key - 4);
    } else if (mode_jump_mash == MODE_MASH) {
      // 1-16 (mash mode)
      // do momentary fx
      fx_toggle[key - 4] = true;
      go_update_fx(key - 4);
    }
  }
}

void button_key_on_double(uint8_t key1, uint8_t key2) {
  printf("on %d+%d\n", key1, key2);
  if (key1 == KEY_SHIFT && key2 > 3) {
    // S+H
    if (mode_jump_mash == MODE_JUMP) {
      // S+H (jump mode)
      // toggles fx
      fx_toggle[key2 - 4] = !fx_toggle[key2 - 4];
      bool on = fx_toggle[key2 - 4];
      go_update_fx(key2 - 4);
    } else if (mode_jump_mash == MODE_MASH) {
      // S+H (mash mode)
      // does jump
      key_do_jump(key2 - 4);
    }
  } else if (key1 > 3 && key2 > 3) {
    // H+H
    if (mode_jump_mash == MODE_JUMP) {
      // retrigger
      go_retrigger_2key(key1, key2);
    }
  } else if (key1 == KEY_A) {
    // A
    if (key2 == KEY_B) {
      // A+B
      mode_samp_bank = 0;
    } else if (key2 == KEY_C) {
      // A+C
      mode_samp_bank = 1;
    } else if (key2 > 3) {
      if (mode_samp_bank == 0) {
        // A+H (sample  mode)
        // select sample
        fil_current_bank_next = fil_current_bank_sel;
        fil_current_id_next =
            ((key2 - 4) % (file_list[fil_current_bank_next].num / 2)) * 2;
        fil_current_change = true;
      } else {
        // A+H (bank mode)
        // select bank
        if (file_list[key2 - 4].num > 0) {
          fil_current_bank_sel = key2 - 4;
        }
      }
    }
  }
}

void button_handler(ButtonMatrix *bm) {
  if (key_total_pressed == 0) {
    key_timer++;
  }
  if (key_timer == 40 && key_pressed_num > 0) {
    printf("combo: ");
    for (uint8_t i = 0; i < key_pressed_num; i++) {
      printf("%d ", key_pressed[i]);
    }
    printf("\n");
    key_timer = 0;
    key_pressed_num = 0;
  }

  // read the latest from the queue
  ButtonMatrix_read(bm);

  // check queue for buttons that turned off
  bool do_update_top = false;
  for (uint8_t i = 0; i < bm->off_num; i++) {
    LEDS_set(leds, LED_PRESS_FACE, bm->off[i], 0);
    key_total_pressed--;
    key_on_buttons[bm->off[i]] = 0;
    button_key_off_any(bm->off[i]);
    // printf("turned off %d\n", bm->off[i]);
    if (key_held_on && (bm->off[i] == key_held_num)) {
      printf("off held %d\n", bm->off[i]);
      button_key_off_held(bm->off[i]);

      key_held_on = false;
      // TODO:
      // use the last key pressed that is still held as the new hold
      if (key_total_pressed > 0) {
        uint16_t *indexes =
            sort_int16_t(key_on_buttons, BUTTONMATRIX_BUTTONS_MAX);
        key_held_on = true;
        key_held_num = indexes[BUTTONMATRIX_BUTTONS_MAX - 1];
        free(indexes);
      } else {
        key_num_presses = 0;
        for (uint8_t i = 0; i < BUTTONMATRIX_BUTTONS_MAX; i++) {
          key_on_buttons[i] = 0;
        }
      }
    } else if (key_held_on) {
      printf("off %d+%d\n", key_held_num, bm->off[i]);
    } else {
      printf("off %d\n", bm->off[i]);
    }

    if (bm->off[i] < 4) {
      do_update_top = true;
    }
  }

  // check queue for buttons that turned on
  bool key_held = false;
  for (uint8_t i = 0; i < bm->on_num; i++) {
    LEDS_set(leds, LED_PRESS_FACE, bm->on[i], 2);
    key_total_pressed++;
    if (!key_held_on) {
      key_held_on = true;
      key_held_num = bm->on[i];
      button_key_on_single(bm->on[i]);
    } else {
      button_key_on_double(key_held_num, bm->on[i]);
    }
    // keep track of combos
    key_pressed[key_pressed_num] = bm->on[i];
    key_pressed_num++;
    key_timer = 0;

    // keep track of all
    key_num_presses++;
    key_on_buttons[bm->on[i]] = key_num_presses;
    if (bm->on[i] < 4) {
      do_update_top = true;
    } else {
      // if 3 are pressed, do retrig
      if (key_total_pressed == 3) {
        uint16_t *indexes =
            sort_int16_t(key_on_buttons, BUTTONMATRIX_BUTTONS_MAX);
        uint8_t keys[3];
        bool all_h = true;
        for (uint8_t i = 0; i < 3; i++) {
          keys[i] = indexes[BUTTONMATRIX_BUTTONS_MAX - 3 + i];
          printf("keys[%d]: %d\n", i, keys[i]);
          if (keys[i] < 4) {
            all_h = false;
          }
        }
        free(indexes);
        if (all_h) {
          // do retrigger!
          go_retrigger_3key(keys[0], keys[1], keys[2]);
        }
      }
    }
  }

  if (do_update_top) {
    go_update_top();
  }
}
