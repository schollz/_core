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
  bm->sm = 0;

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
  sm_config_set_in_shift(&c, false, false, 0);  // Corrected the shift setup
  pio_sm_init(bm->pio, bm->sm, offset, &c);
  pio_sm_set_enabled(bm->pio, bm->sm, true);

  return bm;
}

int ButtonMatrix_read(ButtonMatrix *bm) {
  uint32_t value = 0;

  pio_sm_clear_fifos(bm->pio, bm->sm);
  sleep_ms(1);

  if (pio_sm_is_rx_fifo_empty(bm->pio, bm->sm)) {
    return -1;
  }

  value = pio_sm_get(bm->pio, bm->sm);
  for (int i = 0; i < 16; i++) {
    if ((value & (0x1 << i)) != 0) {
      return i;
    }
  }
  return -1;
}

int main(void) {
  stdio_init_all();

  ButtonMatrix *bm;
  bm = ButtonMatrix_create(10, 18);
  while (1) {
    int key = ButtonMatrix_read(bm);
    if (key >= 0) {
      printf("key pressed = %d\n", key);
    }
    sleep_ms(1000);
  }

  return 0;
}
