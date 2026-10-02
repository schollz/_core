#ifndef CLOCK_STOP_H
#define CLOCK_STOP_H

// Included after the shared clock state in globals.h. Audio and CV clock out
// must agree on clock loss; ordinary audio mutes must not stop clock out.
static inline bool clock_input_should_stop(uint32_t now) {
  return clock_in_do && clock_start_stop_sync &&
         !(use_onewiremidi || usb_midi_present) &&
         (uint32_t)(now - clock_in_last_time) > clock_in_diff_2x;
}

#endif
