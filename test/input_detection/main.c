// Copyright 2023-2025 Zack Scholl, GPLv3.0
// Minimal GPIO_INPUTDETECT test

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "hardware/watchdog.h"
#include "pico/stdlib.h"

// MCP3208 structure and functions (minimal implementation)
typedef struct MCP3208 {
  uint8_t cs_pin;
  uint8_t sck_pin;
  uint8_t mosi_pin;
  uint8_t miso_pin;
  spi_inst_t *spi;
  uint8_t *buffer;
  uint8_t *data;
} MCP3208;

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
  spi_init(spi, 1000 * 1000);

  // Initialize SPI pins
  gpio_set_function(self->sck_pin, GPIO_FUNC_SPI);
  gpio_set_function(self->mosi_pin, GPIO_FUNC_SPI);
  gpio_set_function(self->miso_pin, GPIO_FUNC_SPI);

  return self;
}

uint16_t MCP3208_read(MCP3208 *self, uint8_t channel, bool differential) {
  uint16_t val = 0;

  // Validate channel
  if (channel > 7) {
    printf("[ERROR] Invalid MCP3208 channel: %d\n", channel);
    return 0;
  }

  self->buffer[1] = ((differential ? 0 : 1) << 7) | (channel << 4);
  gpio_put(self->cs_pin, 0);
  int num_bytes_wrote =
      spi_write_read_blocking(self->spi, self->buffer, self->data, 3);
  gpio_put(self->cs_pin, 1);

  if (num_bytes_wrote == 3) {
    val = 1023 - (self->data[2] + (self->data[1] << 8));
  } else {
    printf("[ERROR] MCP3208 SPI read failed: got %d bytes\n", num_bytes_wrote);
  }
  return val;
}

void MCP3208_free(MCP3208 *self) {
  free(self->buffer);
  free(self->data);
  free(self);
}

// CV signal definitions
const uint8_t cv_signals[3] = {MCP_CV_AMEN, MCP_CV_BREAK, MCP_CV_SAMPLE};
bool cv_plugged[3] = {false, false, false};

int main() {
  stdio_init_all();

  // Disable watchdog if it's running (might be causing crashes)
  if (watchdog_caused_reboot()) {
    printf("\n!!! WATCHDOG RESET DETECTED !!!\n");
  }

  // Wait for USB serial connection
  sleep_ms(4000);
  printf("\n=== GPIO_INPUTDETECT Test ===\n");
  printf("Testing input detection for 3 CV inputs\n\n");

  // Initialize GPIO_INPUTDETECT pin
  gpio_init(GPIO_INPUTDETECT);
  gpio_set_dir(GPIO_INPUTDETECT, GPIO_OUT);
  gpio_put(GPIO_INPUTDETECT, 0);

  // Initialize MCP3208 ADC
  // Using SPI1 with pins: CS=9, SCK=10, MOSI=11, MISO=8
  MCP3208 *mcp3208 = MCP3208_malloc(spi1, 9, 10, 11, 8);

  printf("Initialized MCP3208 on SPI1\n");
  printf("GPIO_INPUTDETECT pin: %d\n", GPIO_INPUTDETECT);
  printf("CV channels: AMEN=%d, BREAK=%d, SAMPLE=%d\n\n", MCP_CV_AMEN,
         MCP_CV_BREAK, MCP_CV_SAMPLE);

  // Magic signal patterns for detection
  const uint8_t length_signal = 9;
  uint8_t magic_signal[3][10] = {
      {0, 1, 1, 0, 1, 1, 0, 1, 0, 0},
      {0, 0, 1, 0, 1, 1, 0, 0, 1, 1},
      {1, 0, 0, 1, 0, 1, 0, 1, 1, 1},
  };

  uint16_t debounce_mean_signal = 0;
  uint16_t mean_signal = 0;
  uint16_t debounce_input_detection = 0;
  uint32_t last_mean_signal_print = 0;
  uint32_t loop_counter = 0;
  uint32_t last_heartbeat = 0;

  // Allocate response signal array once (reuse each loop iteration)
  uint8_t response_signal[3][10];

  // Main loop
  while (1) {
    loop_counter++;
    uint32_t current_time = to_ms_since_boot(get_absolute_time());

    // Update watchdog to prevent automatic reset (if enabled)
    if (watchdog_enable_caused_reboot()) {
      watchdog_update();
    }

    // Print heartbeat every 10 seconds to show we're still alive
    if (current_time - last_heartbeat >= 10000) {
      printf("[heartbeat] loop=%lu, time=%lu ms, debounce_mean=%d, debounce_detect=%d\n",
             loop_counter, current_time, debounce_mean_signal, debounce_input_detection);
      last_heartbeat = current_time;
      // Force a flush of stdout to ensure we see this message
      fflush(stdout);
    }

    // Print mean signal every 1 second
    if (current_time - last_mean_signal_print >= 1000) {
      printf("[periodic] t=%lu mean=%d cv=[%d,%d,%d] db=%d\n",
             current_time, mean_signal, cv_plugged[0], cv_plugged[1], cv_plugged[2], debounce_mean_signal);
      last_mean_signal_print = current_time;
      fflush(stdout);
    }

    // Calculate mean signal periodically
    if (debounce_mean_signal > 0 && mean_signal > 0) {
      debounce_mean_signal--;
    } else {
      // Calculate mean signal from unplugged channels
      int16_t total_mean_signal = 0;
      uint8_t total_signals_sent = 0;
      for (uint8_t j = 0; j < 3; j++) {
        if (!cv_plugged[j]) {
          total_signals_sent++;
          for (uint8_t i = 0; i < length_signal; i++) {
            gpio_put(GPIO_INPUTDETECT, magic_signal[j][i]);
            sleep_us(6);
            total_mean_signal += MCP3208_read(mcp3208, cv_signals[j], false);
          }
        }
      }
      if (total_signals_sent > 0) {
        mean_signal = total_mean_signal / (total_signals_sent * length_signal);
        printf("[mean] recalc=%d from=%d\n", mean_signal, total_signals_sent);
        fflush(stdout);
      } else {
        // All channels plugged - can't recalculate, keep using old value
        printf("[mean] all_plugged keep=%d\n", mean_signal);
        fflush(stdout);
      }
      debounce_mean_signal = 10000;
    }

    // Input detection
    if (debounce_input_detection > 0) {
      debounce_input_detection--;
    } else if (mean_signal > 0) {
      int16_t val_input;
      // Clear response signal array
      for (uint8_t j = 0; j < 3; j++) {
        for (uint8_t i = 0; i < 10; i++) {
          response_signal[j][i] = 0;
        }
      }

      // Send magic signals and read responses
      for (uint8_t j = 0; j < 3; j++) {
        // Validate array index
        if (j >= 3) {
          printf("[ERROR] Invalid j index: %d\n", j);
          break;
        }
        for (uint8_t i = 0; i < length_signal; i++) {
          // Validate array index
          if (i >= 10) {
            printf("[ERROR] Invalid i index: %d\n", i);
            break;
          }
          gpio_put(GPIO_INPUTDETECT, magic_signal[j][i]);
          sleep_us(6);
          val_input = MCP3208_read(mcp3208, cv_signals[j], false);
          if (val_input > mean_signal) {
            response_signal[j][i] = 1;
          }
        }
      }

      // Check if responses match magic signals (unplugged) or not (plugged)
      bool is_signal[3] = {true, true, true};
      for (uint8_t j = 0; j < 3; j++) {
        for (uint8_t i = 0; i < length_signal; i++) {
          if (response_signal[j][i] != magic_signal[j][i]) {
            is_signal[j] = false;
            break;
          }
        }
      }

      // Update plug status and print changes
      for (uint8_t j = 0; j < 3; j++) {
        if (!is_signal[j] && !cv_plugged[j]) {
          printf("[cv_%d] PLUGGED\n", j);
          fflush(stdout);
          debounce_mean_signal = 10;
        } else if (is_signal[j] && cv_plugged[j]) {
          printf("[cv_%d] UNPLUGGED\n", j);
          fflush(stdout);
          debounce_mean_signal = 10;
        }
        cv_plugged[j] = !is_signal[j];
      }
      debounce_input_detection = 100;
    }

    sleep_ms(1);
  }

  // Clean up (never reached)
  MCP3208_free(mcp3208);
  return 0;
}
