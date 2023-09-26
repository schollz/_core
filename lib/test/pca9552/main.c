#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hardware/i2c.h"
#include "pico/binary_info.h"
#include "pico/stdlib.h"
//
#include "../../pca9552.h"

#define I2C_SDA_PIN 20
#define I2C_SCL_PIN 21

int main(void) {
  stdio_init_all();
  sleep_ms(1000);

  i2c_init(i2c_default, 400 * 1000);
  gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
  gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
  gpio_pull_up(I2C_SDA_PIN);
  gpio_pull_up(I2C_SCL_PIN);

  PCA9552 *pca;
  pca = PCA9552_create(0x60, i2c_default);
  if (pca->error != PCA9552_OK) {
    printf("PCA9552_ERROR: %02x\n", pca->error);
  }

  while (1) {
    for (uint8_t i = 0; i < 16; i++) {
      PCA9553_ledSet(pca, i, 0);
      sleep_ms(500);
      PCA9553_ledSet(pca, i, 1);
      sleep_ms(500);
      PCA9553_ledSet(pca, i, 2);
      sleep_ms(500);
      PCA9553_ledSet(pca, i, 0);
    }
    // sleep_ms(1000);
    // PCA9552_writeReg(pca, 0x06, 0x00);
    // PCA9552_writeReg(pca, 0x07, 0x55);
    // PCA9552_writeReg(pca, 0x08, 0x00);
    // PCA9552_writeReg(pca, 0x09, 0x6C);
    // sleep_ms(1000);
    // PCA9552_writeReg(pca, 0x06, 0x55);
    // PCA9552_writeReg(pca, 0x07, 0x00);
    // PCA9552_writeReg(pca, 0x08, 0x6C);
    // PCA9552_writeReg(pca, 0x09, 0x00);
  }
  free(pca);
  return 0;
}
