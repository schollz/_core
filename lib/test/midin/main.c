#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "bsp/board.h"
#include "hardware/gpio.h"
#include "pico/binary_info.h"
#include "pico/bootrom.h"
#include "pico/stdlib.h"
#include "tusb.h"

uint32_t read_midi_message(uint8_t* buffer, uint32_t bufsize) {
  if (tud_midi_n_available(0, 0)) {
    uint32_t num_bytes_read = tud_midi_n_stream_read(0, 0, buffer, bufsize);
    return num_bytes_read;  // Return the number of bytes actually read
  }
  return 0;
}

uint32_t send_text_as_sysex(const char* text) {
  uint32_t text_length = strlen(text);  // Get the length of the text
  uint8_t sysex_data[text_length + 2];  // +2 for SysEx start and end bytes

  sysex_data[0] = 0xF0;  // Start of SysEx
  for (uint32_t i = 0; i < text_length; i++) {
    sysex_data[i + 1] = text[i];  // Copy text into SysEx data
  }
  sysex_data[text_length + 1] = 0xF7;  // End of SysEx

  // Call the stream write function with the SysEx message
  return tud_midi_n_stream_write(0, 0, sysex_data, sizeof(sysex_data));
}

void send_midi_note_on(uint8_t note, uint8_t velocity) {
  // Ensure TinyUSB stack is initialized and ready
  if (tud_ready()) {
    // MIDI cable number 0, Note On event, channel 1
    uint8_t channel = 0;  // MIDI channels are 0-15

    // Construct the MIDI message
    // MIDI Note On message format: 0x9n, where n is the channel number
    uint8_t midi_message[3];
    midi_message[0] = 0x90 | channel;  // Note On command with channel
    midi_message[1] = note;            // MIDI note number
    midi_message[2] = velocity;        // Note velocity

    // Send the MIDI message
    tud_midi_n_stream_write(0, 0, midi_message, sizeof(midi_message));
  }
}

int main() {
  stdio_init_all();

  // set gpio 25 to output
  gpio_init(25);
  gpio_set_dir(25, GPIO_OUT);
  tusb_init();
  uint8_t midi_buffer[64];
  while (1) {
    tud_task();  // tinyusb device task

    // Read a MIDI message from the USB MIDI stream
    uint32_t bytes_read = read_midi_message(midi_buffer, sizeof(midi_buffer));
    if (bytes_read == 3) {
      // Extract the status byte and MIDI channel
      uint8_t status = midi_buffer[0] & 0xF0;

      // Extract the note number and velocity
      uint8_t note = midi_buffer[1];
      uint8_t velocity = midi_buffer[2];
      if (note == 0 && velocity == 0 && status == 0x80) {
        reset_usb_boot(0, 0);
      }
      send_text_as_sysex("hello, world");
    }

    // write sysex

    // uint8_t packet[4];
    // bool read_midi = tud_midi_n_packet_read(0, packet);
    // if (read_midi) {
    //   // increase the note by 1 and send it back
    //   packet[2] += 1;
    //   // increase velocity by 1 and send it back
    //   packet[3] += 1;

    //   packet[1] = 0x90;

    //   // change packet to note on event
    //   packet[0] = 0x09;
    //   packet[1] = 0x90;
    //   packet[2] = 0x3C;
    //   packet[3] = 0x7F;
    //   tud_midi_n_packet_write(0, packet);
    // }
    sleep_ms(1);
  }
  return 0;
}
