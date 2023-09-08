#include <stdio.h>
#include <stdlib.h>

#include "hardware/pio.h"
#include "pico/stdlib.h"
//
#include "../../buttonmatrix.h"

int main(void) {
  stdio_init_all();

  ButtonMatrix *bm;
  bm = ButtonMatrix_create(5, 9);
  while (1) {
    ButtonMatrix_read(bm);
    if (ButtonMatrix_changed(bm)) {
      printf("pressed %d buttons\n", ButtonMatrix_num_pressed(bm));
      // ButtonMatrix_print_history(bm);
      ButtonMatrix_print_buttons(bm);
    }
    sleep_ms(1);
  }

  return 0;
}
