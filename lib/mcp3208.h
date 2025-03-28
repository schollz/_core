// Copyright 2023-2025 Zack Scholl, GPLv3.0

#ifndef LIB_MCP3208_H
#define LIB_MCP3208_H 1

#include <stdint.h>

typedef struct MCP3208 {
  uint8_t cs_pin;
  uint8_t sck_pin;
  uint8_t mosi_pin;
  uint8_t miso_pin;
  spi_inst_t *spi;
  uint8_t *buffer;
  uint8_t *data;
} MCP3208;

void MCP3208_free(MCP3208 *self) { free(self); }

MCP3208 *MCP3208_malloc(spi_inst_t *spi, uint8_t cs_pin, uint8_t sck_pin,
                        uint8_t mosi_pin, uint8_t miso_pin) {
  MCP3208 *self = (MCP3208 *)malloc(sizeof(MCP3208));
  self->cs_pin = cs_pin;
  self->sck_pin = sck_pin;
  self->mosi_pin = mosi_pin;
  self->miso_pin = miso_pin;
  self->spi = spi;
  self->buffer = (uint8_t *)malloc(3 * sizeof(uint8_t));
  self->data = (uint8_t *)malloc(3 * sizeof(uint8_t));

  self->buffer[0] = 0x01;
  self->buffer[2] = 0x00;

  // Initialize CS pin high
  gpio_init(self->cs_pin);
  gpio_set_dir(self->cs_pin, GPIO_OUT);
  gpio_put(self->cs_pin, 1);

  // Initialize SPI port at 1 MHz
  spi_init(spi1, 1000 * 1000);

  // Initialize SPI pins
  gpio_set_function(self->sck_pin, GPIO_FUNC_SPI);
  gpio_set_function(self->mosi_pin, GPIO_FUNC_SPI);
  gpio_set_function(self->miso_pin, GPIO_FUNC_SPI);

  return self;
}

uint16_t MCP3208_read(MCP3208 *self, uint8_t channel, bool differential) {
  uint16_t val = 0;
  self->buffer[1] = ((differential ? 0 : 1) << 7) | (channel << 4);
  gpio_put(self->cs_pin, 0);
  int num_bytes_wrote =
      spi_write_read_blocking(self->spi, self->buffer, self->data, 3);
  gpio_put(self->cs_pin, 1);

  if (num_bytes_wrote == 3) {
    val = 1023 - (self->data[2] + (self->data[1] << 8));
  }
  return val;
}

#endif