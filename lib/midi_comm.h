#ifndef LIB_MIDI_COMM_H
#define LIB_MIDI_COMM_H 1
#include <stdarg.h>  // Include this header for va_start, va_end, etc.
#include <stdio.h>   // Include for vsnprintf

uint8_t midi_buffer[64];

uint32_t read_midi_message(uint8_t* buffer, uint32_t bufsize) {
  if (tud_midi_n_available(0, 0)) {
    uint32_t num_bytes_read = tud_midi_n_stream_read(0, 0, buffer, bufsize);
    return num_bytes_read;  // Return the number of bytes actually read
  }
  return 0;
}

uint32_t send_buffer_as_sysex(uint8_t* buffer, uint32_t bufsize) {
  uint8_t sysex_data[bufsize + 2];  // +2 for SysEx start and end bytes

  sysex_data[0] = 0xF0;  // Start of SysEx
  for (uint32_t i = 0; i < bufsize; i++) {
    sysex_data[i + 1] = buffer[i];  // Copy buffer into SysEx data
  }
  sysex_data[bufsize + 1] = 0xF7;  // End of SysEx

  return tud_midi_n_stream_write(0, 0, sysex_data, sizeof(sysex_data));
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

int printf_sysex(const char* format, ...) {
  if (!tud_ready()) {
    return 0;
  }
  va_list args;
  va_start(args, format);
  int text_length = vsnprintf(NULL, 0, format, args);
  va_end(args);

  char text[text_length + 1];  // +1 for null terminator
  va_start(args, format);
  vsnprintf(text, text_length + 1, format, args);
  va_end(args);

  return send_text_as_sysex(text);
}

typedef void (*midi_comm_callback)(uint8_t, uint8_t, uint8_t, uint8_t);

void midi_comm_task(midi_comm_callback callback) {
  // Read a MIDI message from the USB MIDI stream
  uint32_t bytes_read = read_midi_message(midi_buffer, sizeof(midi_buffer));
  if (bytes_read == 3) {
    // Extract the status byte and MIDI channel
    uint8_t status = midi_buffer[0] & 0xF0;
    uint8_t channel = midi_buffer[0] & 0x0F;

    // Extract the note number and velocity
    uint8_t note = midi_buffer[1];
    uint8_t velocity = midi_buffer[2];
    if (status == 176 && channel == 0 && note == 0) {
      send_text_as_sysex("command=reset");
      sleep_ms(10);
      reset_usb_boot(0, 0);
    } else if (status == 176 && channel == 0 && note == 1) {
      send_text_as_sysex("version=v2.3.1");
    }
    if (callback != NULL) {
      callback(status, channel, note, velocity);
    }
  }
}

#define printf printf_sysex
#endif