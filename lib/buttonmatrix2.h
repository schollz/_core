// Copyright 2023-2025 Zack Scholl, GPLv3.0

#include "buttonmatrix2.pio.h"
#include "sort.h"

#define BUTTONMATRIX_BUTTONS_MAX 20
#define BUTTONMATRIX_ROWS 5
#define BUTTONMATRIX_COLS 4

typedef struct ButtonMatrix {
  PIO pio;
  uint sm;
  uint8_t mapping[BUTTONMATRIX_BUTTONS_MAX];
  int16_t on_buttons[BUTTONMATRIX_BUTTONS_MAX];
  uint16_t on[BUTTONMATRIX_BUTTONS_MAX];  // list which buttons are on
  uint16_t num_presses;
  bool changed;
  bool changed_on;
  bool changed_off;
  uint8_t num_pressed;
  uint32_t last_value;
} ButtonMatrix;

inline uint8_t count_ones(uint32_t n) {
  int count = 0;
  while (n) {
    n &= (n - 1);
    count++;
  }
  return count;
}

void ButtonMatrix_dec_to_binary(ButtonMatrix *bm, uint32_t num) {
  if (num == 0) {
    printf("\n");
  }
  for (uint8_t i = 0; i < BUTTONMATRIX_BUTTONS_MAX; i++) {
    // if ((num >> i) & 1) {
    //   printf("%d ", bm->mapping[i]);
    // }
    uint8_t bit = (num >> i) & 1;
    printf("%u", bit);  // Print the bit
  }
  printf("\n");
}

void ButtonMatrix_reset(ButtonMatrix *bm) {
  bm->num_presses = 0;
  for (uint8_t i = 0; i < BUTTONMATRIX_BUTTONS_MAX; i++) {
    bm->on_buttons[i] = -1;
    bm->on[i] = -1;
  }
  return;
}

void ButtonMatrix_print_buttons(ButtonMatrix *bm) {
  for (uint8_t i = 0; i < BUTTONMATRIX_BUTTONS_MAX; i++) {
    printf("%d) %d\n", i, bm->on_buttons[i]);
  }
  printf("\n");
  return;
}

ButtonMatrix *ButtonMatrix_create(uint base_input, uint base_output) {
  ButtonMatrix *bm = (ButtonMatrix *)malloc(sizeof(ButtonMatrix));
  bm->pio = pio0;
  bm->sm = 1;
  bm->changed = false;
  bm->num_pressed = 0;
  bm->last_value = 0;

  for (int i = 0; i < BUTTONMATRIX_ROWS; i++) {
    pio_gpio_init(bm->pio, base_input + i);
    gpio_pull_down(base_input + i);
  }

  for (int i = 0; i < BUTTONMATRIX_COLS; i++) {
    pio_gpio_init(bm->pio, base_output + i);
  }

  ButtonMatrix_reset(bm);
  bm->mapping[0] = 3;
  bm->mapping[1] = 7;
  bm->mapping[2] = 11;
  bm->mapping[3] = 15;
  bm->mapping[4] = 19;
  bm->mapping[5] = 2;
  bm->mapping[6] = 6;
  bm->mapping[7] = 10;
  bm->mapping[8] = 14;
  bm->mapping[9] = 18;
  bm->mapping[10] = 1;
  bm->mapping[11] = 5;
  bm->mapping[12] = 9;
  bm->mapping[13] = 13;
  bm->mapping[14] = 17;
  bm->mapping[15] = 0;
  bm->mapping[16] = 4;
  bm->mapping[17] = 8;
  bm->mapping[18] = 12;
  bm->mapping[19] = 16;

  for (uint8_t i = 0; i < BUTTONMATRIX_BUTTONS_MAX; i++) {
    bm->on_buttons[i] = -1;  // -1 == off
  }

  uint offset = pio_add_program(bm->pio, &button_matrix_program);
  pio_sm_config c = button_matrix_program_get_default_config(offset);
  sm_config_set_in_pins(&c, base_input);
  pio_sm_set_consecutive_pindirs(bm->pio, bm->sm, base_output,
                                 BUTTONMATRIX_COLS, true);
  sm_config_set_set_pins(&c, base_output, BUTTONMATRIX_COLS);
  sm_config_set_in_shift(&c, 0, 0, 0);  // Corrected the shift setup
  pio_sm_init(bm->pio, bm->sm, offset, &c);
  pio_sm_set_enabled(bm->pio, bm->sm, true);

  return bm;
}

void ButtonMatrix_read(ButtonMatrix *bm) {
  bm->changed = false;
  bm->changed_on = false;
  uint32_t value = 0;

  pio_sm_clear_fifos(bm->pio, bm->sm);
  sleep_ms(1);

  if (pio_sm_is_rx_fifo_empty(bm->pio, bm->sm)) {
    printf("fifo empty\n");
    return;
  }

  value = pio_sm_get(bm->pio, bm->sm);
  if (value != bm->last_value) {
    bm->changed = true;
    if (value != bm->last_value) {
      bm->num_presses++;
      bm->num_pressed = count_ones(value);
      for (uint8_t i = 0; i < BUTTONMATRIX_BUTTONS_MAX; i++) {
        uint8_t j = bm->mapping[i];
        if ((value >> i) & 1) {
          // button turned off to on
          if (bm->on_buttons[j] == -1) {
            printf("on: %d\n", j);
            bm->on_buttons[j] = bm->num_presses;
          }
        } else if (bm->on_buttons[j] > -1) {
          // button turned on to off
          printf("off: %d\n", j);
          bm->on_buttons[j] = -1;
        }
      }
      if (value == 0) {
        bm->num_pressed = 0;
      }
      uint8_t j = 0;
      uint16_t *indexes =
          sort_int16_t(bm->on_buttons, BUTTONMATRIX_BUTTONS_MAX);
      for (uint8_t i = 0; i < BUTTONMATRIX_BUTTONS_MAX; i++) {
        if (bm->on_buttons[indexes[i]] > -1) {
          bm->on[j] = indexes[i];
          j++;
        }
      }
      free(indexes);
    }
    if (value > bm->last_value) {
      bm->changed_on = true;
    } else if (value < bm->last_value) {
      bm->changed_off = true;
    }
    bm->last_value = value;
  }
}

void ButtonMatrix_print(ButtonMatrix *bm, uint32_t num) {
  // Iterate through each bit position (from 31 to 0)
  if (num == 0) {
    printf("\n");
  }
  for (uint8_t i = 0; i < BUTTONMATRIX_BUTTONS_MAX; i++) {
    if ((num >> i) & 1) {
      printf("%d ", bm->mapping[i]);
    }
    // uint8_t bit = (num >> i) & 1;
    // printf("%u", bit);  // Print the bit
  }
}

bool ButtonMatrix_changed(ButtonMatrix *bm) { return bm->changed; }
uint8_t ButtonMatrix_num_pressed(ButtonMatrix *bm) { return bm->num_pressed; }
