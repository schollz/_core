// Copyright 2026 Zack Scholl, GPLv3.0
#include "sharp_memory.h"

#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "hardware/dma.h"
#include "pico/stdlib.h"

// Protocol: Sharp LS027B7DH01 specification, sections 6-5 and 6-6.
// https://learn.adafruit.com/adafruit-sharp-memory-display-breakout/downloads
enum {
  PIN_CLK = 26,
  PIN_DI = 27,
  PIN_CS = 28,
  COMMAND_WRITE = 0x01,
  COMMAND_VCOM = 0x02,
  COMMAND_CLEAR = 0x04,
};

static uint8_t vcom;
static uint32_t vcom_at;
static int tx_dma = -1;
static bool sending;
static uint8_t packet[1 + 8 * (SHARP_MEMORY_ROW_BYTES + 2) + 1];

static uint8_t reverse_bits(uint8_t value) {
  value = (uint8_t)((value >> 4) | (value << 4));
  value = (uint8_t)(((value & 0xcc) >> 2) | ((value & 0x33) << 2));
  return (uint8_t)(((value & 0xaa) >> 1) | ((value & 0x55) << 1));
}

static void begin_command(uint8_t command) {
  // Hardware SPI only supports MSB-first. Reverse bytes for this LSB-first LCD.
  if ((uint32_t)(time_us_32() - vcom_at) >= 500000u) {
    vcom ^= COMMAND_VCOM;
    vcom_at = time_us_32();
  }
  uint8_t wire_command = reverse_bits(command | vcom);
  gpio_put(PIN_CS, 1);
  sleep_us(6);  // Sharp specifies at least 3 us from CS rising to clock.
  spi_write_blocking(spi1, &wire_command, 1);
}

static void end_command(void) {
  const uint8_t dummy = 0;
  spi_write_blocking(spi1, &dummy, 1);
  sleep_us(2);
  gpio_put(PIN_CS, 0);
  sleep_us(2);
}

void sharp_memory_init(void) {
  gpio_init(PIN_CS);
  gpio_put(PIN_CS, 0);
  gpio_set_dir(PIN_CS, GPIO_OUT);

  spi_init(spi1, 1000000);
  spi_set_format(spi1, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
  gpio_set_function(PIN_CLK, GPIO_FUNC_SPI);
  gpio_set_function(PIN_DI, GPIO_FUNC_SPI);
  vcom = 0;
  vcom_at = time_us_32();
  sleep_ms(100);  // Allow the breakout's supply and boost converter to settle.
}

void sharp_memory_clear(void) {
  begin_command(COMMAND_CLEAR);
  end_command();
}

bool sharp_memory_async_init(void) {
  if (tx_dma >= 0) return true;
  tx_dma = dma_claim_unused_channel(false);
  return tx_dma >= 0;
}

bool sharp_memory_busy(void) { return sending; }

void sharp_memory_poll(void) {
  if (sending) {
    if (dma_channel_is_busy(tx_dma) || spi_is_busy(spi1)) return;
    while (spi_is_readable(spi1)) (void)spi_get_hw(spi1)->dr;
    spi_get_hw(spi1)->icr = SPI_SSPICR_RORIC_BITS;
    sleep_us(2);
    gpio_put(PIN_CS, 0);
    sending = false;
  }
  if ((uint32_t)(time_us_32() - vcom_at) >= 500000u) {
    begin_command(0);
    end_command();
  }
}

bool sharp_memory_submit_rows(unsigned first, unsigned count, const uint8_t *rows) {
  if (tx_dma < 0 || sending || !count || count > 8 || first + count > SHARP_MEMORY_HEIGHT)
    return false;
  // begin_command sends the command synchronously (8 us); only pixel payload
  // is DMA-fed. The packet remains immutable until poll sees SPI idle.
  size_t n = 0;
  for (unsigned y = 0; y < count; ++y) {
    packet[n++] = reverse_bits((uint8_t)(first + y + 1));
    for (unsigned x = 0; x < SHARP_MEMORY_ROW_BYTES; ++x)
      packet[n++] = reverse_bits(rows[y * SHARP_MEMORY_ROW_BYTES + x]);
    packet[n++] = 0;
  }
  packet[n++] = 0;
  begin_command(COMMAND_WRITE);
  dma_channel_config config = dma_channel_get_default_config(tx_dma);
  channel_config_set_transfer_data_size(&config, DMA_SIZE_8);
  channel_config_set_read_increment(&config, true);
  channel_config_set_write_increment(&config, false);
  channel_config_set_dreq(&config, spi_get_dreq(spi1, true));
  sending = true;
  dma_channel_configure(tx_dma, &config, &spi_get_hw(spi1)->dr, packet, n, true);
  return true;
}

void sharp_memory_refresh(const uint8_t *framebuffer) {
  uint8_t line[SHARP_MEMORY_ROW_BYTES + 2];
  begin_command(COMMAND_WRITE);
  for (unsigned y = 0; y < SHARP_MEMORY_HEIGHT; ++y) {
    line[0] = reverse_bits((uint8_t)(y + 1));
    for (unsigned x = 0; x < SHARP_MEMORY_ROW_BYTES; ++x) {
      line[x + 1] = reverse_bits(framebuffer[y * SHARP_MEMORY_ROW_BYTES + x]);
    }
    line[SHARP_MEMORY_ROW_BYTES + 1] = 0;
    spi_write_blocking(spi1, line, sizeof(line));
  }
  end_command();
}
