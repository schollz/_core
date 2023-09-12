#include <stdio.h>
#include <string.h>

#include "hardware/i2c.h"
#include "pico/binary_info.h"
#include "pico/stdlib.h"

#define I2C_SDA_PIN 0
#define I2C_SCL_PIN 1

#define PCA9552_WRITE 0xC0
#define PCA9552_READ 0xC1

#define PCA9552_ADDR 0x60  // 7-bit address

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

int reg_write(i2c_inst_t *i2c, const uint addr, const uint8_t reg, uint8_t *buf,
              const uint8_t nbytes) {
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
  return i2c_write_timeout_us(i2c, addr, msg, (nbytes + 1), false, 10000);
}

// I2C reserves some addresses for special purposes. We exclude these from the
// scan. These are any addresses of the form 000 0xxx or 111 1xxx
bool reserved_addr(uint8_t addr) {
  return (addr & 0x78) == 0 || (addr & 0x78) == 0x78;
}

int main(void) {
  stdio_init_all();
  sleep_ms(2000);
#if !defined(i2c_default) || !defined(PICO_DEFAULT_I2C_SDA_PIN) || \
    !defined(PICO_DEFAULT_I2C_SCL_PIN)
#warning i2c/bus_scan example requires a board with I2C pins
  puts("Default I2C pins were not defined");
#else
  // This example will use I2C0 on the default SDA and SCL pins (GP4, GP5 on a
  // Pico)
  i2c_init(i2c_default, 100 * 1000);
  gpio_set_function(0, GPIO_FUNC_I2C);
  gpio_set_function(1, GPIO_FUNC_I2C);
  gpio_pull_up(0);
  gpio_pull_up(1);
  // Make the I2C pins available to picotool
  // bi_decl(bi_2pins_with_func(0, 1, GPIO_FUNC_I2C));

  printf("\n\n\nI2C Bus Scan\n");
  printf("   0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\n");

  for (int addr = 0; addr < (1 << 7); ++addr) {
    if (addr % 16 == 0) {
      printf("%02x ", addr);
    }
    sleep_ms(10);

    // Perform a 1-byte dummy read from the probe address. If a slave
    // acknowledges this address, the function returns the number of bytes
    // transferred. If the address byte is ignored, the function returns
    // -1.

    // Skip over any reserved addresses.
    int ret;
    uint8_t rxdata;
    if (reserved_addr(addr))
      ret = PICO_ERROR_GENERIC;
    else
      ret = i2c_read_timeout_us(i2c_default, addr, &rxdata, 1, false, 10000);

    printf(ret < 0 ? "." : "@");
    printf(addr % 16 == 15 ? "\n" : "  ");
  }
  printf("Done.\n");

  printf("\ni2c_write_blocking test\n");
  printf("   0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\n");

  for (int addr = 0; addr < (1 << 7); ++addr) {
    if (addr % 16 == 0) {
      printf("%02x ", addr);
    }

    int ret;
    uint8_t txdata = 0;
    if (reserved_addr(addr)) {
      ret = PICO_ERROR_GENERIC;
    } else {
      ret = i2c_write_timeout_us(i2c_default, addr, &txdata, 1, false, 10000);
      /*if (ret != 1) {
         printf("never printed as ret is always 1\n");
      }*/
    }
    sleep_ms(10);

    printf(ret < 0 ? "." : "@");
    printf(addr % 16 == 15 ? "\n" : "  ");
  }
  printf("Done.\n");
  return 0;
#endif

  // // Ports
  // i2c_inst_t *i2c = i2c0;

  // Initialize I2C port at 400 kHz
  i2c_init(i2c_default, 400 * 1000);

  gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
  gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
  gpio_pull_up(I2C_SDA_PIN);
  gpio_pull_up(I2C_SCL_PIN);

  // Read device ID to make sure that we can communicate with the ADXL343
  // reg_read(i2c, PCA9552_ADDR, PC, data, 1);
  // if (data[0] != DEVID) {
  //   printf("ERROR: Could not communicate with ADXL343\r\n");
  //   while (true)
  //     ;
  // }

  static const uint8_t REG_POWER_CTL = 0x2D;
  sleep_ms(2000);
  printf("\n\n\n");
  uint8_t data[6];
  while (true) {
    sleep_ms(1000);
    printf("--------\n");
    printf("I2C_SDA_PIN: %d, ", I2C_SDA_PIN);
    printf("I2C_SCL_PIN: %d, ", I2C_SCL_PIN);
    printf("i2c: %d\n", i2c_default);
    uint8_t msg[1];
    msg[0] = 0xC0;  // control register (select WRITE)
    // msg[1] = 0x06;  // select first four leds
    // msg[2] = 0x1B;  // change them all
    printf("write 1\n");

    // address is one of the following
    // 1100000  0x60
    // 11000000 0xC0
    // 0000011  0x03
    // 1100111  0x67
    // 11001110 0xCE

    // command byte is
    // 00000110 0x06
    // 00010110 0x16
    data[0] = 0x1B;
    for (uint8_t addr = 0; addr < 255; addr++) {
      int res = reg_write(i2c_default, addr, 0x16, &data[0], 1);
      if (res != -1) {
        printf("addr: %x, res: %d\n", addr, res);
        sleep_ms(100);
      }
      sleep_ms(1);
    }

    msg[0] = 0x06;  // control register (select WRITE)
    printf("write 2\n");
    printf("wrote: %d\n", i2c_write_blocking(i2c_default, 0x60, msg, 1, false));
    msg[0] = 0x1B;  // control register (select WRITE)
    printf("write 3\n");
    printf("wrote: %d\n", i2c_write_blocking(i2c_default, 0x60, msg, 1, false));
  }
  return 0;
}
