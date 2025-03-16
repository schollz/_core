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
  bool button_on_last[BUTTONMATRIX_BUTTONS_MAX];
  bool button_on[BUTTONMATRIX_BUTTONS_MAX];
  int8_t on_num;
  uint8_t on[BUTTONMATRIX_BUTTONS_MAX];  // list of buttons that turned n
  int8_t off_num;
  uint8_t off[BUTTONMATRIX_BUTTONS_MAX];  // list of buttons that turned off
  uint32_t held_time[BUTTONMATRIX_BUTTONS_MAX];
  uint32_t off_time[BUTTONMATRIX_BUTTONS_MAX];
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

#ifdef INCLUDE_ZEPTOMECH
  bm->mapping[0] = 0;
  bm->mapping[1] = 4;
  bm->mapping[2] = 8;
  bm->mapping[3] = 12;
  bm->mapping[4] = 16;

  bm->mapping[5] = 1;
  bm->mapping[6] = 5;
  bm->mapping[7] = 9;
  bm->mapping[8] = 13;
  bm->mapping[9] = 17;

  bm->mapping[10] = 2;
  bm->mapping[11] = 6;
  bm->mapping[12] = 10;
  bm->mapping[13] = 14;
  bm->mapping[14] = 18;

  bm->mapping[15] = 3;
  bm->mapping[16] = 7;
  bm->mapping[17] = 11;
  bm->mapping[18] = 15;
  bm->mapping[19] = 19;

#endif

  for (uint8_t i = 0; i < BUTTONMATRIX_BUTTONS_MAX; i++) {
    bm->button_on[i] = false;  // -1 == off
    bm->button_on_last[i] = false;
    bm->held_time[i] = 0;
    bm->off_time[i] = 0;
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
  uint32_t ct = to_ms_since_boot(get_absolute_time());

  if (is_arcade_box) {
    // input_pin_state0 corresponds to the first 4 buttons
    uint8_t input_pin_state_a =
        mcp23017_get_pins_gpioa(i2c_default, MCP23017_ADDR1);
    sleep_us(100);
    uint8_t input_pin_state_b =
        mcp23017_get_pins_gpiob(i2c_default, MCP23017_ADDR1);
    sleep_us(100);
    uint8_t input_pin_state_c =
        mcp23017_get_pins_gpioa(i2c_default, MCP23017_ADDR2);
    sleep_us(100);
    for (uint8_t i = 0; i < 4; i++) {
      // check if bit is set
      bm->button_on[i] = (input_pin_state_c >> i) & 1;
    }
    for (uint8_t i = 0; i < 8; i++) {
      // check if bit is set
      bm->button_on[i + 4] = (input_pin_state_a >> i) & 1;
    }
    for (uint8_t i = 0; i < 8; i++) {
      // check if bit is set
      bm->button_on[i + 12] = (input_pin_state_b >> i) & 1;
    }

    // reverse
    for (uint8_t i = 0; i < BUTTONMATRIX_BUTTONS_MAX; i++) {
      bm->button_on[i] = !bm->button_on[i];
      // preventing stuck key problems
      if (bm->button_on[i] && bm->held_time[i] == 0) {
        bm->held_time[i] = ct;
      } else if (bm->button_on[i] && bm->held_time[i] > 0) {
        if (ct - bm->held_time[i] > 5000) {
          bm->button_on[i] = false;
        }
      } else if (!bm->button_on[i]) {
        bm->held_time[i] = 0;
      }
    }

    // check if any buttons changed
    for (uint8_t i = 0; i < BUTTONMATRIX_BUTTONS_MAX; i++) {
      if (bm->button_on[i] != bm->button_on_last[i]) {
        // printf("[zeptocade buttonmatrix3.h] button %d changed to %d\n", i,
        //        bm->button_on[i]);
        if (bm->button_on[i]) {
          bm->on_num++;
          bm->on[bm->on_num] = i;
          bm->held_time[i] = ct;
        } else {
          bm->off_num++;
          bm->off_time[i] = ct;
          bm->off[bm->off_num] = i;
        }
      }
    }

    // save as last state
    for (uint8_t i = 0; i < BUTTONMATRIX_BUTTONS_MAX; i++) {
      bm->button_on_last[i] = bm->button_on[i];
    }

    if (bm->on_num > -1) {
      bm->on_num++;
    }
    if (bm->off_num > -1) {
      bm->off_num++;
    }
    return;
  }

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
        bm->off_time[j] = ct;
        bm->on[bm->on_num] = j;
        // printf("[%d] on: %d\n", bm->on_num, j);
      }
    } else if (bm->button_on[j]) {
      // button turned on to off
      bm->button_on[j] = false;
      bm->off_num++;
      bm->off_time[j] = ct;
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
