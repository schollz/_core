#include <stdio.h>
#include <stdlib.h>

#include "pico/stdlib.h"

typedef struct Charlieplex {
  uint8_t gpios[5];
  uint8_t startpoints[20];
  uint8_t endpoints[20];
  uint8_t values[20];
  uint8_t row;
} Charlieplex;

Charlieplex *Charlieplex_create() {
  Charlieplex *cp = (Charlieplex *)malloc(sizeof(Charlieplex));
  cp->gpios[0] = 0;
  cp->gpios[1] = 1;
  cp->gpios[2] = 2;
  cp->gpios[3] = 3;
  cp->gpios[4] = 4;
  cp->startpoints[0] = 0;
  cp->startpoints[1] = 1;
  cp->startpoints[2] = 0;
  cp->startpoints[3] = 2;
  cp->startpoints[4] = 1;
  cp->startpoints[5] = 2;
  cp->startpoints[6] = 0;
  cp->startpoints[7] = 3;
  cp->startpoints[8] = 1;
  cp->startpoints[9] = 3;
  cp->startpoints[10] = 2;
  cp->startpoints[11] = 3;
  cp->startpoints[12] = 1;
  cp->startpoints[13] = 4;
  cp->startpoints[14] = 2;
  cp->startpoints[15] = 4;
  cp->startpoints[16] = 3;
  cp->startpoints[17] = 4;
  cp->startpoints[18] = 0;
  cp->startpoints[19] = 4;

  cp->endpoints[0] = 1;
  cp->endpoints[1] = 0;
  cp->endpoints[2] = 2;
  cp->endpoints[3] = 0;
  cp->endpoints[4] = 2;
  cp->endpoints[5] = 1;
  cp->endpoints[6] = 3;
  cp->endpoints[7] = 0;
  cp->endpoints[8] = 3;
  cp->endpoints[9] = 1;
  cp->endpoints[10] = 3;
  cp->endpoints[11] = 2;
  cp->endpoints[12] = 4;
  cp->endpoints[13] = 1;
  cp->endpoints[14] = 4;
  cp->endpoints[15] = 2;
  cp->endpoints[16] = 4;
  cp->endpoints[17] = 3;
  cp->endpoints[18] = 4;
  cp->endpoints[19] = 0;

  for (uint8_t i = 0; i < 20; i++) {
    cp->values[i] = 0;
  }

  for (uint8_t i = 0; i < 5; i++) {
    gpio_init(cp->gpios[i]);
    gpio_set_dir(cp->gpios[i], GPIO_IN);
    gpio_pull_down(cp->gpios[i]);
  }

  cp->row = 0;

  return cp;
}

void Charlieplex_update(Charlieplex *cp) {
  bool do_light = false;
  for (uint8_t i = 0; i < 20; i++) {
    if (cp->values[i] > 0 && cp->startpoints[i] == cp->row) {
      do_light = true;
      break;
    }
  }
  if (do_light) {
    for (uint8_t i = 0; i < 5; i++) {
      gpio_set_dir(cp->gpios[i], GPIO_IN);
    }

    gpio_set_dir(cp->gpios[cp->row], GPIO_OUT);
    gpio_put(cp->gpios[cp->row], 1);

    for (uint8_t i = 0; i < 20; i++) {
      if (cp->values[i] > 0 && cp->startpoints[i] == cp->row) {
        gpio_set_dir(cp->gpios[cp->endpoints[i]], GPIO_OUT);
        gpio_put(cp->gpios[cp->endpoints[i]], 0);
      }
    }
  }

  // update the curent row
  cp->row++;
  if (cp->row == 5) {
    cp->row = 0;
  }
  return;
}

void Charlieplex_set(Charlieplex *cp, uint8_t pin, uint8_t val) {
  cp->values[pin] = val;
  return;
}

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
    for (uint8_t i = 0; i < 20; i++) {
      Charlieplex_set(cp, i, 0);
    }
    Charlieplex_set(cp, c, 1);
    Charlieplex_update(cp);

    d++;
    if (d == 0) {
      printf("c: %d\n", c);
      c++;
      if (c == 20) {
        c = 0;
      }
    }
  }
  return 0;
}
