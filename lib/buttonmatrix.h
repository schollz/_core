#include "buttonmatrix.pio.h"

#define BUTTONMATRIX_HISTORY_MAX 10
#define BUTTONMATRIX_BUTTONS_MAX 16

typedef struct ButtonMatrix {
  PIO pio;
  uint sm;
  uint8_t mapping[BUTTONMATRIX_BUTTONS_MAX];
  int16_t on[BUTTONMATRIX_BUTTONS_MAX];
  int16_t press;
  bool changed;
  uint8_t pressed;
} ButtonMatrix;

uint8_t count_ones(uint32_t n) {
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

void ButtonMatrix_reset_history(ButtonMatrix *bm) {
  bm->i = 0;
  for (uint8_t i = 0; i < BUTTONMATRIX_HISTORY_MAX; i++) {
    bm->history[i] = 0;
  }
  for (uint8_t i = 0; i < BUTTONMATRIX_BUTTONS_MAX; i++) {
    bm->on[i] = -1;
  }
  return;
}

void ButtonMatrix_print_history(ButtonMatrix *bm) {
  for (uint8_t i = 0; i < bm->i; i++) {
    ButtonMatrix_dec_to_binary(bm, bm->history[i]);
  }
  printf("\n");
  return;
}

void ButtonMatrix_print_buttons(ButtonMatrix *bm) {
  for (uint8_t i = 0; i < BUTTONMATRIX_BUTTONS_MAX; i++) {
    printf("%d) %d\n", i, bm->on[i]);
  }
  printf("\n");
  return;
}

ButtonMatrix *ButtonMatrix_create(uint base_input, uint base_output) {
  ButtonMatrix *bm = (ButtonMatrix *)malloc(sizeof(ButtonMatrix));
  bm->pio = pio0;
  bm->sm = 1;
  bm->changed = false;
  bm->pressed = 0;

  for (int i = 0; i < 4; i++) {
    pio_gpio_init(bm->pio, base_output + i);
    pio_gpio_init(bm->pio, base_input + i);
    gpio_pull_down(base_input + i);
  }

  ButtonMatrix_reset_history(bm);
  bm->mapping[0] = 3;
  bm->mapping[1] = 7;
  bm->mapping[2] = 11;
  bm->mapping[3] = 15;
  bm->mapping[4] = 2;
  bm->mapping[5] = 6;
  bm->mapping[6] = 10;
  bm->mapping[7] = 14;
  bm->mapping[8] = 1;
  bm->mapping[9] = 5;
  bm->mapping[10] = 9;
  bm->mapping[11] = 13;
  bm->mapping[12] = 0;
  bm->mapping[13] = 4;
  bm->mapping[14] = 8;
  bm->mapping[15] = 12;

  for (uint8_t i = 0; i < BUTTONMATRIX_BUTTONS_MAX; i++) {
    bm->on[i] = -1;  // -1 == off
  }

  uint offset = pio_add_program(bm->pio, &button_matrix_program);
  pio_sm_config c = button_matrix_program_get_default_config(offset);
  sm_config_set_in_pins(&c, base_input);
  pio_sm_set_consecutive_pindirs(bm->pio, bm->sm, base_output, 4, true);
  sm_config_set_set_pins(&c, base_output, 4);
  sm_config_set_in_shift(&c, 0, 0, 0);  // Corrected the shift setup
  pio_sm_init(bm->pio, bm->sm, offset, &c);
  pio_sm_set_enabled(bm->pio, bm->sm, true);

  return bm;
}

void ButtonMatrix_read(ButtonMatrix *bm) {
  bm->changed = false;
  uint32_t value = 0;

  pio_sm_clear_fifos(bm->pio, bm->sm);
  sleep_ms(1);

  if (pio_sm_is_rx_fifo_empty(bm->pio, bm->sm)) {
    return;
  }

  value = pio_sm_get(bm->pio, bm->sm);
  if (value == 0 && bm->i > 0) {
    ButtonMatrix_reset_history(bm);
    bm->changed = true;
    bm->pressed = 0;
  } else if (value > 0 && bm->i < BUTTONMATRIX_HISTORY_MAX) {
    if (bm->i > 0) {
      if (value == bm->history[bm->i - 1]) {
        return;
      }
    }
    bm->history[bm->i] = value;
    bm->changed = true;
    bm->pressed = count_ones(value);
    for (uint8_t i = 0; i < BUTTONMATRIX_BUTTONS_MAX; i++) {
      uint8_t j = bm->mapping[i];
      if ((value >> i) & 1) {
        // button turned off to on
        if (bm->on[j] == -1) {
          bm->on[j] = bm->i;
        }
      } else if (bm->on[j] > -1) {
        // button turned on to off
        bm->on[j] = -1;
      }
    }
    if (bm->i == 0 || (bm->i > 0 && value > bm->history[bm->i - 1])) {
      bm->i++;
    }
    printf("bm->i: %d\n", bm->i);
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
uint8_t ButtonMatrix_num_pressed(ButtonMatrix *bm) { return bm->pressed; }