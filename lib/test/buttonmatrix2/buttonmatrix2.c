#include <stdio.h>
#include <stdlib.h>

#include "../../sort.h"
#include "hardware/pio.h"
#include "pico/stdlib.h"
//
#include "../../buttonmatrix2.h"

int main(void) {
  stdio_init_all();

  ButtonMatrix *bm;
  bm = ButtonMatrix_create(1, 6);
  while (1) {
    ButtonMatrix_read(bm);
    if (bm->changed) {
      for (uint8_t i = 0; i < bm->num_pressed; i++) {
        printf("%d ", bm->on[i]);
      }
      printf("\n");
    }
    sleep_ms(1);
  }

  return 0;
}
