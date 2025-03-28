#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "hardware/i2c.h"
#include "pico/stdlib.h"
//
#include "../../mcp23017/mcp23017_lib.h"

#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)                                \
  ((byte)&0x80 ? '1' : '0'), ((byte)&0x40 ? '1' : '0'),     \
      ((byte)&0x20 ? '1' : '0'), ((byte)&0x10 ? '1' : '0'), \
      ((byte)&0x08 ? '1' : '0'), ((byte)&0x04 ? '1' : '0'), \
      ((byte)&0x02 ? '1' : '0'), ((byte)&0x01 ? '1' : '0')

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
  printf("MCP23017 test\n");
#define mcp23017_addr 0x20
#define mcp23017_addr2 0x21
  i2c_inst_t *mcp23017_i2c = i2c0;
  mcp23017_init(mcp23017_i2c, mcp23017_addr);
  mcp23017_init(mcp23017_i2c, mcp23017_addr2);
  // addr1
  // set all as inputs
  mcp23017_set_dir_gpioa(mcp23017_i2c, mcp23017_addr, 0b11111111);
  mcp23017_set_dir_gpiob(mcp23017_i2c, mcp23017_addr, 0b11111111);
  mcp23017_set_pullup_gpioa(mcp23017_i2c, mcp23017_addr, 0b11111111);
  mcp23017_set_pullup_gpiob(mcp23017_i2c, mcp23017_addr, 0b11111111);
  // addr2
  // set B as outputs and A as inputs
  mcp23017_set_dir_gpioa(mcp23017_i2c, mcp23017_addr2, 0b11111111);
  mcp23017_set_dir_gpiob(mcp23017_i2c, mcp23017_addr2, 0b00000000);
  mcp23017_set_pullup_gpioa(mcp23017_i2c, mcp23017_addr2, 0b11111111);

  // check if it exists
  uint8_t rxdata;
  uint8_t ret =
      i2c_read_timeout_us(i2c_default, 0x20, &rxdata, 1, false, 10000);
  printf("ret: %d\n", ret);

  while (1) {
    sleep_ms(100);
    uint8_t input_pin_state_a =
        mcp23017_get_pins_gpioa(mcp23017_i2c, mcp23017_addr);
    printf(BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(input_pin_state_a));
    uint8_t input_pin_state_b =
        mcp23017_get_pins_gpiob(mcp23017_i2c, mcp23017_addr);
    printf(BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(input_pin_state_b));
    printf("\n");

    // mirror
    uint8_t input_pin_state =
        mcp23017_get_pins_gpioa(mcp23017_i2c, mcp23017_addr2);
    mcp23017_set_pins_gpiob(mcp23017_i2c, mcp23017_addr2, ~input_pin_state);
  }

  return 0;
}
