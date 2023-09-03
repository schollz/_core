#include <stdio.h>
#include <stdlib.h>

#include "buttonmatrix.pio.h"
#include "hardware/pio.h"
#include "pico/stdlib.h"

typedef struct ButtonMatrix {
  PIO pio;
  uint sm;
} ButtonMatrix;

ButtonMatrix *ButtonMatrix_create(uint base_input, uint base_output) {
  ButtonMatrix *bm = (ButtonMatrix *)malloc(sizeof(ButtonMatrix));
  bm->pio = pio0;
  bm->sm = 1;

  for (int i = 0; i < 4; i++) {
    pio_gpio_init(bm->pio, base_output + i);
    pio_gpio_init(bm->pio, base_input + i);
    gpio_pull_down(base_input + i);
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

uint32_t ButtonMatrix_read(ButtonMatrix *bm) {
  uint32_t value = 0;

  pio_sm_clear_fifos(bm->pio, bm->sm);
  sleep_ms(1);

  if (pio_sm_is_rx_fifo_empty(bm->pio, bm->sm)) {
    return -1;
  }

  value = pio_sm_get(bm->pio, bm->sm);
  // for (int i = 0; i < 16; i++) {
  //   if ((value & (0x1 << i)) != 0) {
  //     return i;
  //   }
  // }
  return value;
}

void printBinaryRepresentation(uint32_t num) {
  // Iterate through each bit position (from 31 to 0)
  for (int i = 31; i >= 0; i--) {
    // Extract the i-th bit using bitwise operations
    uint8_t bit = (num >> i) & 1;
    printf("%u", bit);  // Print the bit
  }
  printf("\n");
}

int main(void) {
  stdio_init_all();

  ButtonMatrix *bm;
  bm = ButtonMatrix_create(5,9);
  uint32_t keys_last = 0;
  while (1) {
    uint32_t keys = ButtonMatrix_read(bm);
    if (keys != keys_last) {
      printBinaryRepresentation(keys);
      keys_last = keys;
    }
    sleep_ms(1);
  }

  return 0;
}
