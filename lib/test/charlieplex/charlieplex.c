

#include <stdio.h>
#include <stdlib.h>
//
#include "pico/stdlib.h"
//
#include "../../charlieplex.h"

int main(void) {
  stdio_init_all();

  // for (uint8_t i = 0; i < 5; i++) {
  //   gpio_init(i);
  //   gpio_pull_down(i);
  // }
  // gpio_set_dir(2, GPIO_IN);
  // gpio_set_dir(3, GPIO_IN);
  // gpio_set_dir(4, GPIO_IN);
  // gpio_set_dir(1, GPIO_OUT);
  // gpio_put(1, 0);
  // gpio_set_dir(0, GPIO_OUT);
  // gpio_put(0, 1);

  Charlieplex *cp;
  cp = Charlieplex_create();
  uint8_t c = 0;
  uint8_t d = 0;

  while (true) {
    sleep_ms(1);

    // reset all
    Charlieplex_update(cp);

    d++;
    if (d == 0) {
      printf("c: %d\n", c);
      c++;
      if (c == 16) {
        c = 0;
      }
      Charlieplex_toggle(cp, c);
    }
  }
  return 0;
}
