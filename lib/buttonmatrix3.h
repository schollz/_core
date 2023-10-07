#include "buttonmatrix3.pio.h"
#include "sort.h"

#define BUTTONMATRIX_BUTTONS_MAX 20
#define BUTTONMATRIX_ROWS 5
#define BUTTONMATRIX_COLS 4

typedef struct ButtonMatrix {
  PIO pio;
  uint sm;
  uint32_t last_value;
  uint8_t mapping[BUTTONMATRIX_BUTTONS_MAX];
  bool button_on[BUTTONMATRIX_BUTTONS_MAX];
  int8_t on_num;
  uint8_t on[BUTTONMATRIX_BUTTONS_MAX];  // list of buttons that turned n
  int8_t off_num;
  uint8_t off[BUTTONMATRIX_BUTTONS_MAX];  // list of buttons that turned off
} ButtonMatrix;

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

void ButtonMatrix_print_buttons(ButtonMatrix *bm) {
  for (uint8_t i = 0; i < BUTTONMATRIX_BUTTONS_MAX; i++) {
    printf("%d) %d\n", i, bm->button_on[i]);
  }
  printf("\n");
  return;
}

ButtonMatrix *ButtonMatrix_create(uint base_input, uint base_output) {
  ButtonMatrix *bm = (ButtonMatrix *)malloc(sizeof(ButtonMatrix));
  bm->pio = pio0;
  bm->sm = 1;
  bm->last_value = 0;
  bm->on_num = -1;
  bm->off_num = -1;

  for (int i = 0; i < BUTTONMATRIX_ROWS; i++) {
    pio_gpio_init(bm->pio, base_input + i);
    gpio_pull_down(base_input + i);
  }

  for (int i = 0; i < BUTTONMATRIX_COLS; i++) {
    pio_gpio_init(bm->pio, base_output + i);
  }

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
    bm->button_on[i] = false;  // -1 == off
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
  bm->on_num = -1;
  bm->off_num = -1;

  // read new value;
  uint32_t value = 0;
  pio_sm_clear_fifos(bm->pio, bm->sm);
  sleep_ms(1);
  if (pio_sm_is_rx_fifo_empty(bm->pio, bm->sm)) {
    printf("fifo empty\n");
    return;
  }
  value = pio_sm_get(bm->pio, bm->sm);

  if (value == bm->last_value) {
    return;
  }
  for (uint8_t i = 0; i < BUTTONMATRIX_BUTTONS_MAX; i++) {
    uint8_t j = bm->mapping[i];
    if ((value >> i) & 1) {
      // button turned off to on
      if (!bm->button_on[j]) {
        bm->button_on[j] = true;
        bm->on_num++;
        bm->on[bm->on_num] = j;
        // printf("[%d] on: %d\n", bm->on_num, j);
      }
    } else if (bm->button_on[j]) {
      // button turned on to off
      bm->button_on[j] = false;
      bm->off_num++;
      bm->off[bm->off_num] = j;
      // printf("[%d] off: %d\n", bm->off_num, j);
    }
  }
  if (bm->on_num > -1) {
    bm->on_num++;
  }
  if (bm->off_num > -1) {
    bm->off_num++;
  }
  bm->last_value = value;
}
