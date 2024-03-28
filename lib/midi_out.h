
#ifndef MIDIOUT_LIB
#define MIDIOUT_LIB 1

typedef struct MidiOut {
  uint8_t channel : 7;
  uint8_t monophonic : 1;
  int8_t last;
} MidiOut;

MidiOut *MidiOut_malloc(uint8_t channel, bool monophonic) {
  MidiOut *self = (MidiOut *)malloc(sizeof(MidiOut));
  self->channel = channel;
  self->monophonic = monophonic;
  self->last = -1;
  return self;
}

void MidiOut_free(MidiOut *self) { free(self); }

void MidiOut_on(MidiOut *self, uint8_t note, uint8_t velocity) {
  if (self->monophonic && self->last != -1) {
    tud_midi_n_stream_write(
        0, 0, (uint8_t[3]){0x80 | (self->channel & 0x0F), self->last, 0}, 3);
  }
  tud_midi_n_stream_write(
      0, 0, (uint8_t[3]){0x90 | (self->channel & 0x0F), note, velocity}, 3);
  self->last = note;
}

void MidiOut_off(MidiOut *self, uint8_t note) {
  tud_midi_n_stream_write(
      0, 0, (uint8_t[3]){0x80 | (self->channel & 0x0F), note, 0}, 3);
  if (self->last == note) {
    self->last = -1;
  }
}

#endif
