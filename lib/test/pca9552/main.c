#include <stdio.h>
#include <string.h>

#include "hardware/i2c.h"
#include "pico/binary_info.h"
#include "pico/stdlib.h"

// The bus address is determined by the state of pins A0, A1 and A2 on the
// MCP9808 board
static uint8_t ADDRESS = 0x18;

#define I2C_SDA_PIN 0
#define I2C_SCL_PIN 1
#define i2c_default 0

#define PCA9552_WRITE 0xC0
#define PCA9552_READ 0xC1

#define PCA9552_ADDR 0x62

//  REGISTERS
#define PCA9552_INPUT0 0x00
#define PCA9552_INPUT1 0x01
#define PCA9552_PSC0 0x02
#define PCA9552_PWM0 0x03
#define PCA9552_PSC1 0x04
#define PCA9552_PWM1 0x05
#define PCA9552_LS0 0x06
#define PCA9552_LS1 0x07
#define PCA9552_LS2 0x08
#define PCA9552_LS3 0x09

//  MUX OUTPUT MODES
#define PCA9552_MODE_LOW 0
#define PCA9552_MODE_HIGH 1
#define PCA9552_MODE_PWM0 2
#define PCA9552_MODE_PWM1 3

//  ERROR CODES (not all used yet)
#define PCA9552_OK 0x00
#define PCA9552_ERROR 0xFF
#define PCA9552_ERR_WRITE 0xFE
#define PCA9552_ERR_CHAN 0xFD
#define PCA9552_ERR_MODE 0xFC
#define PCA9552_ERR_REG 0xFB
#define PCA9552_ERR_I2C 0xFA

// Write 1 byte to the specified register
int reg_write(i2c_inst_t *i2c, const uint addr, const uint8_t reg, uint8_t *buf,
              const uint8_t nbytes) {
  int num_bytes_read = 0;
  uint8_t msg[nbytes + 1];

  // Check to make sure caller is sending 1 or more bytes
  if (nbytes < 1) {
    return 0;
  }

  // Append register address to front of data packet
  msg[0] = reg;
  for (int i = 0; i < nbytes; i++) {
    msg[i + 1] = buf[i];
  }

  // Write data to register(s) over I2C
  i2c_write_blocking(i2c, addr, msg, (nbytes + 1), false);

  return num_bytes_read;
}

// Read byte(s) from specified register. If nbytes > 1, read from consecutive
// registers.
int reg_read(i2c_inst_t *i2c, const uint addr, const uint8_t reg, uint8_t *buf,
             const uint8_t nbytes) {
  int num_bytes_read = 0;

  // Check to make sure caller is asking for 1 or more bytes
  if (nbytes < 1) {
    return 0;
  }

  // Read data from register(s) over I2C
  i2c_write_blocking(i2c, addr, &reg, 1, true);
  num_bytes_read = i2c_read_blocking(i2c, addr, buf, nbytes, false);

  return num_bytes_read;
}

int main(void) {
  stdio_init_all();

  // Ports
  i2c_inst_t *i2c = i2c0;

  // Initialize I2C port at 400 kHz
  i2c_init(i2c, 400 * 1000);

  gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
  gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
  gpio_pull_up(I2C_SDA_PIN);
  gpio_pull_up(I2C_SCL_PIN);

  sleep_ms(1000);
  printf("I2C_SDA_PIN: %d", I2C_SDA_PIN);
  printf("I2C_SCL_PIN: %d", I2C_SCL_PIN);
  printf("i2c_default: %d", i2c_default);

  sleep_ms(1000);
  // Read device ID to make sure that we can communicate with the ADXL343
  reg_read(i2c, PCA9552_ADDR, REG_DEVID, data, 1);
  if (data[0] != DEVID) {
    printf("ERROR: Could not communicate with ADXL343\r\n");
    while (true)
      ;
  }

  while (true) {
    sleep_ms(1000);
  }
  return 0;
}
