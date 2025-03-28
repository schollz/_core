#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "hardware/i2c.h"
#include "pico/stdlib.h"
//
#include "../../ads7830.h"

#define I2C_SDA_PIN 20
#define I2C_SCL_PIN 21

int main(void) {
  stdio_init_all();

  i2c_init(i2c_default, 50 * 1000);
  gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
  gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
  gpio_pull_up(I2C_SDA_PIN);
  gpio_pull_up(I2C_SCL_PIN);

  sleep_ms(1000);
  printf("ADS7830 test\n");
#define ADS7380_ADDR 0x48

  uint8_t rxdata;
  uint8_t ret =
      i2c_read_timeout_us(i2c_default, ADS7380_ADDR, &rxdata, 1, false, 10000);
  printf("ret: %d\n", ret);
  ADS7830 *adc = ADS7830_malloc(ADS7380_ADDR);

  while (1) {
    sleep_ms(100);
    // create string from all the values
    char str[128];
    // clear str
    str[0] = '\0';
    for (uint8_t i = 0; i < 8; i++) {
      uint8_t adcValue = ADS7830_read(adc, i);
      sprintf(str, "%s%03d ", str, adcValue);
    }
    sprintf(str, "%s\n\0", str);
    printf("%s", str);
  }

  return 0;
}
