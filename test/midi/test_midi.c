// Exercise the production USB dispatcher and slice-note callbacks with host
// substitutes for USB, sample metadata, transport, and the audio jump boundary.
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define FILEZERO 0
typedef struct
{
  uint8_t slice_num;
  int32_t *slice_start, *slice_stop;
} SampleInfo;
typedef struct
{
  SampleInfo *snd[2];
} Sample;
typedef struct
{
  uint16_t num_samples;
  Sample *sample;
} SampleList;

static int32_t boundaries[255];
static SampleInfo sounds[2] = {{16, boundaries, boundaries},
                               {7, boundaries, boundaries}};
static Sample samples[2] = {{{&sounds[0], NULL}}, {{&sounds[1], NULL}}};
static SampleList bank = {2, samples};
static SampleList *banks[16] = {&bank};
static uint8_t sel_bank_cur, sel_sample_cur;
static int sel_variation, sel_variation_next;
static bool playback_stopped, do_stop_playback, button_mute, trigger_button_mute;
static bool fil_is_open = true, fil_current_change, fil_current_change_force;
static bool do_open_file_ready, media_allowed = true, usb_midi_present;
static bool audio_media_timer_allowed(void) { return media_allowed; }
static unsigned jumps;
static int last_slice, last_beat;
static void key_do_jump_to_slice(int32_t slice, uint8_t beat)
{
  jumps++;
  last_slice = slice;
  last_beat = beat;
}
#include "midi_note_key.h"

typedef void (*callback_int_int)(int, int);
typedef void (*callback_int)(int);
typedef void (*callback_void)(void);
typedef void (*callback_uint8_uint8_uint8)(uint8_t, uint8_t, uint8_t);
static uint8_t packets[64][4];
static unsigned packet_count, packet_index;
static unsigned starts, stops, continues, clocks, note_ons, note_offs, ccs, resets;
static unsigned generic_count;
static uint8_t generic_event[4], cc_event[3];
static char sysex[128];
static unsigned stream_writes;
static bool mounted = true;
static bool tud_mounted(void) { return mounted; }
static bool tud_ready(void) { return true; }
static uint32_t tud_midi_n_stream_write(uint8_t itf, uint8_t cable,
                                        const uint8_t *data, uint32_t count)
{
  assert(itf == 0 && cable == 0);
  ++stream_writes;
  if (count > 1 && data[0] == 0xf0)
  {
    assert(count - 2 < sizeof sysex);
    memcpy(sysex, data + 1, count - 2);
    sysex[count - 2] = 0;
  }
  return count;
}
static bool tud_midi_n_packet_read(uint8_t itf, uint8_t packet[4])
{
  assert(itf == 0);
  if (packet_index == packet_count)
    return false;
  memcpy(packet, packets[packet_index++], 4);
  return true;
}
static void sleep_ms(unsigned ms) { assert(ms == 10); }
static void reset_usb_boot(unsigned a, unsigned b)
{
  assert(a == 0 && b == 0);
  resets++;
}
#include "midi_comm.h"
#undef printf

static void on(int note, int velocity)
{
  note_ons++;
  midi_note_on(note, velocity);
}
static void off(int note)
{
  note_offs++;
  midi_note_off(note);
}
static void start(void)
{
  starts++;
  playback_stopped = false;
  do_stop_playback = false;
}
static void stop(void)
{
  stops++;
  do_stop_playback = true;
}
static void resume(void)
{
  continues++;
  start();
}
static void clock_tick(void) { clocks++; }
static void cc(uint8_t channel, uint8_t number, uint8_t value)
{
  ccs++;
  cc_event[0] = channel;
  cc_event[1] = number;
  cc_event[2] = value;
}
static void generic(uint8_t status, uint8_t channel, uint8_t note, uint8_t velocity)
{
  generic_count++;
  generic_event[0] = status;
  generic_event[1] = channel;
  generic_event[2] = note;
  generic_event[3] = velocity;
}
static void queue(uint8_t cin, uint8_t a, uint8_t b, uint8_t c)
{
  assert(packet_count < 64);
  uint8_t *p = packets[packet_count++];
  p[0] = cin;
  p[1] = a;
  p[2] = b;
  p[3] = c;
}
static void drain(void)
{
  while (packet_index < packet_count)
    midi_comm_task(generic, on, off, start, resume, stop, clock_tick, cc);
  packet_count = packet_index = 0;
}

#ifdef INCLUDE_ZEPTOCORE
static void command(const char *text)
{
  uint8_t bytes[256];
  size_t length = strlen(text);
  assert(length + 2 <= sizeof bytes);
  bytes[0] = 0xf0;
  memcpy(bytes + 1, text, length);
  bytes[++length] = 0xf7;
  ++length;
  for (size_t i = 0; i < length;)
  {
    size_t count = length - i;
    if (count > 3)
      count = 3;
    uint8_t cin = i + count < length ? 4 : (uint8_t)(4 + count);
    queue(cin, bytes[i], count > 1 ? bytes[i + 1] : 0, count > 2 ? bytes[i + 2] : 0);
    i += count;
  }
}
#endif

static void test_notes(void)
{
  unsigned before = jumps;
  midi_note_on(60, 100);
#if MIDI_NOTE_KEY == 1
  assert(jumps == before + 1 && last_slice == 12 && last_beat == 12);
  // All pitches wrap correctly, including a sample with more than 128 slices.
  const uint8_t counts[] = {1, 7, 16, 31, 255};
  for (unsigned c = 0; c < sizeof counts; c++)
  {
    sounds[0].slice_num = counts[c];
    for (int note = 0; note < 128; note++)
    {
      before = jumps;
      midi_note_on(note, 1);
      assert(jumps == before + 1);
      assert(last_slice == note % counts[c] && last_beat == last_slice % 16);
      midi_note_on(note, 127);
      assert(jumps == before + 2 && last_slice == note % counts[c]);
    }
  }
  sounds[0].slice_num = 16;
  sel_sample_cur = 1;
  midi_note_on(60, 100);
  assert(last_slice == 4); // New sample has seven slices.
  sel_sample_cur = 0;
  before = jumps;
  button_mute = trigger_button_mute = true;
  midi_note_on(63, 100);
  assert(jumps == before + 1 && last_slice == 15);
  assert(button_mute && trigger_button_mute && !playback_stopped);
  button_mute = trigger_button_mute = false;
#else
  assert(jumps == before);
#endif
  before = jumps;
  midi_note_off(60);
  midi_note_on(60, 0);
  midi_note_on(-1, 100);
  midi_note_on(128, 100);
  midi_note_on(60, 128);
  assert(jumps == before && !button_mute && !trigger_button_mute);

  bool *blocked[] = {&playback_stopped, &do_stop_playback, &fil_current_change,
                     &fil_current_change_force, &do_open_file_ready};
  for (unsigned i = 0; i < sizeof blocked / sizeof blocked[0]; i++)
  {
    *blocked[i] = true;
    midi_note_on(60, 100);
    assert(*blocked[i] && jumps == before);
    *blocked[i] = false;
  }
  fil_is_open = false;
  midi_note_on(60, 100);
  fil_is_open = true;
  media_allowed = false;
  midi_note_on(60, 100);
  media_allowed = true;
  sel_variation_next = 1;
  midi_note_on(60, 100);
  sel_variation_next = 0;
  sel_bank_cur = 16;
  midi_note_on(60, 100);
  sel_bank_cur = 1;
  midi_note_on(60, 100);
  sel_bank_cur = 0;
  sel_sample_cur = 2;
  midi_note_on(60, 100);
  sel_sample_cur = 0;
  bank.sample = NULL;
  midi_note_on(60, 100);
  bank.sample = samples;
  samples[0].snd[0] = NULL;
  midi_note_on(60, 100);
  samples[0].snd[0] = &sounds[0];
  sounds[0].slice_num = 0;
  midi_note_on(60, 100);
  sounds[0].slice_num = 16;
  sounds[0].slice_start = NULL;
  midi_note_on(60, 100);
  sounds[0].slice_start = boundaries;
  sounds[0].slice_stop = NULL;
  midi_note_on(60, 100);
  sounds[0].slice_stop = boundaries;
  assert(jumps == before);
  assert(banks[0] == &bank && sel_bank_cur == 0 && sel_sample_cur == 0);
  assert(sel_variation == 0 && sel_variation_next == 0 && fil_is_open);
  assert(last_beat >= 0 && last_beat < 16);
}

static void test_usb(void)
{
  unsigned before = jumps;
  // Same USB transfer: clock, full note, more clock, repeated note, releases.
  queue(0x0f, 0xfa, 0, 0);
  queue(0x0f, 0xf8, 0, 0);
  queue(0x09, 0x90, 60, 100);
  queue(0x0f, 0xf8, 0, 0);
  queue(0x09, 0x90, 60, 64);
  queue(0x08, 0x80, 60, 64);
  queue(0x09, 0x90, 61, 0);
  queue(0x0f, 0xfc, 0, 0);
  queue(0x09, 0x90, 63, 100); // Pending Stop must suppress this jump.
  queue(0x0f, 0xfb, 0, 0);
  queue(0x09, 0x90, 62, 127);
  queue(0x0b, 0xb0, 7, 64);
  drain();
  assert(starts == 2 && continues == 1 && stops == 1 && clocks == 2);
  assert(note_ons == 4 && note_offs == 2 && usb_midi_present);
  assert(ccs == 1 && cc_event[0] == 0 && cc_event[1] == 7 && cc_event[2] == 64);
  assert(jumps == before + (MIDI_NOTE_KEY == 1 ? 3 : 0));
  if (MIDI_NOTE_KEY == 1)
    assert(last_slice == 14);

  before = jumps;
  for (uint8_t channel = 1; channel < 16; channel++)
  {
    queue(0x09, 0x90 | channel, 60, 100);
    queue(0x08, 0x80 | channel, 60, 64);
  }
  drain();
#ifdef INCLUDE_ZEPTOCORE
  assert(jumps == before && generic_count == 0);
  command("core_cmd=1,key,49");
  drain();
#else
  assert(jumps == before && generic_count == 30);
#endif
  queue(0x08, 0x89, 49, 1); // Existing channel 10 command, unchanged payload.
  drain();
  assert(generic_event[0] == 0x80 && generic_event[1] == 9 &&
         generic_event[2] == 49 && generic_event[3] == 1);
#ifdef INCLUDE_ZEPTOCORE
  command("core_cmd=1,version");
#else
  queue(0x0b, 0xb0, 1, 0);
#endif
  drain();
  assert(strcmp(sysex, "version=v8.0.4") == 0);
#ifdef INCLUDE_ZEPTOCORE
  command("core_cmd=1,bootloader");
#else
  queue(0x0b, 0xb0, 0, 0);
#endif
  drain();
  assert(resets == 1 && strcmp(sysex, "command=reset") == 0);

  unsigned on_before = note_ons, off_before = note_offs, cc_before = ccs;
  unsigned generic_before = generic_count;
  queue(0x19, 0x90, 60, 100);  // Unadvertised cable.
  queue(0x00, 0x90, 60, 100);  // Reserved CIN.
  queue(0x09, 0x80, 60, 100);  // CIN/status mismatch.
  queue(0x09, 0x90, 128, 100); // Invalid data byte.
  queue(0x09, 0x90, 60, 128);
  queue(0x04, 0xf0, 60, 100); // SysEx and its fragments never become commands.
  queue(0x04, 0x90, 60, 100);
  queue(0x07, 0x89, 49, 1);
  queue(0x05, 0xf7, 0, 0);
  queue(0x0c, 0xc0, 5, 0); // Two-byte messages don't consume the next event.
  queue(0x0d, 0xd0, 42, 0);
  queue(0x02, 0xf1, 0, 0);
  drain();
  assert(note_ons == on_before && note_offs == off_before && ccs == cc_before);
  assert(generic_count == generic_before && jumps == before);
  queue(0x09, 0x90, 60, 100);
  drain();
  assert(note_ons == on_before + 1);
  // Empty polls and absent optional callbacks are safe.
  midi_comm_task(NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
  queue(0x0b, 0xb0, 7, 64);
  midi_comm_task(NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
}

#ifdef INCLUDE_ZEPTOCORE
static void test_receive_channels(void)
{
  do_stop_playback = playback_stopped = false;
  for (unsigned selected = 1; selected <= 16; ++selected)
  {
    midi_receive_channel = selected;
    for (unsigned channel = 0; channel < 16; ++channel)
    {
      unsigned on_before = note_ons, off_before = note_offs, cc_before = ccs;
      unsigned generic_before = generic_count, reset_before = resets;
      usb_midi_present = false;
      queue(9, 0x90 | channel, 60, 100);
      queue(8, 0x80 | channel, 60, 1);
      queue(9, 0x90 | channel, 60, 0);
      queue(11, 0xb0 | channel, 7, 64);
      drain();
      bool match = channel == selected - 1;
      assert(note_ons == on_before + match && note_offs == off_before + 2 * match);
      assert(ccs == cc_before + match && usb_midi_present == match);
      assert(generic_count == generic_before && resets == reset_before);
    }
    unsigned clocks_before = clocks, starts_before = starts, stops_before = stops;
    queue(15, 0xf8, 0, 0);
    queue(15, 0xfa, 0, 0);
    queue(15, 0xfc, 0, 0);
    queue(15, 0xfb, 0, 0);
    drain();
    assert(clocks == clocks_before + 1 && starts == starts_before + 2 && stops == stops_before + 1);
  }
  midi_receive_channel = 10;
  unsigned generic_before = generic_count, reset_before = resets;
  for (unsigned note = 0; note < 128; ++note)
    for (unsigned velocity = 0; velocity < 128; ++velocity)
    {
      queue(8, 0x89, note, velocity);
      drain();
    }
  midi_receive_channel = 1;
  queue(11, 0xb0, 0, 0);
  queue(11, 0xb0, 1, 0);
  drain();
  assert(generic_count == generic_before && resets == reset_before);
  usb_midi_present = false;
  queue(14, 0xe0, 0, 64);
  queue(10, 0xa0, 60, 64);
  drain();
  assert(!usb_midi_present);
}
static void test_commands(void)
{
  midi_receive_channel = 10;
  command("core_cmd=1,hello");
  drain();
  assert(strcmp(sysex, "core_caps=1") == 0);
  const char *operations[] = {"info", "slices", "view", "key,0", "key,127"};
  for (unsigned i = 0; i < 5; ++i)
  {
    char text[64];
    snprintf(text, sizeof text, "core_cmd=1,%s", operations[i]);
    unsigned count = generic_count;
    command(text);
    drain();
    assert(generic_count == count + 1);
  }
  unsigned before = generic_count, writes = stream_writes, reset_before = resets;
  const char *invalid[] = {"core_cmd=2,bootloader", "core_cmd=1,unknown", "core_cmd=1,key,",
                           "core_cmd=1,key,-1", "core_cmd=1,key,128", "core_cmd=1,key,1000", "core_cmd=1,key,1x",
                           "core_cmd=1,key,1,2", "core_cmd=1,bootloader,x", "other_cmd=1,bootloader",
                           "core_cmd=1,xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"};
  for (unsigned i = 0; i < sizeof invalid / sizeof *invalid; ++i)
  {
    command(invalid[i]);
    drain();
  }
  // Complete payload without F7 is inert. Disconnect clears it.
  command("core_cmd=1,key,49");
  --packet_count;
  drain();
  mounted = false;
  midi_comm_task(generic, on, off, start, resume, stop, clock_tick, cc);
  mounted = true;
  queue(5, 0xf7, 0, 0);
  drain();
  // Malformed continuation must not supply the ending of a prior command.
  command("core_cmd=1,key,49");
  --packet_count;
  drain();
  queue(4, 0x01, 0x02, 0x03);
  queue(5, 0xf7, 0, 0);
  drain();
  assert(generic_count == before && stream_writes == writes && resets == reset_before);
  command("core_cmd=1,key,49");
  --packet_count;
  drain();
  // New start abandons a partial frame; realtime does not.
  command("core_cmd=1,hello");
  for (unsigned i = 0; i + 1 < packet_count; ++i)
    midi_comm_task(generic, on, off, start, resume, stop, clock_tick, cc);
  // Insert realtime ahead of the final packet.
  uint8_t end[4];
  memcpy(end, packets[packet_count - 1], 4);
  memcpy(packets[packet_count - 1], (uint8_t[]){15, 0xf8, 0, 0}, 4);
  queue(end[0], end[1], end[2], end[3]);
  drain();
  assert(strcmp(sysex, "core_caps=1") == 0 && generic_count == before);
  midi_receive_channel = 1;
}
#endif

#if ZV_ENABLED
static void visualizer_only_probe(void) {}
static unsigned realtime_written;
static bool realtime_sink(const uint8_t packet[4])
{
  const uint8_t expected[] = {0xfa, 0xf8, 0xfc};
  assert(packet[0] == 15 && realtime_written < 3);
  assert(packet[1] == expected[realtime_written++]);
  return true;
}
#endif
static void test_clock_output(void)
{
  unsigned before = stream_writes;
  send_midi_start();
  send_midi_clock();
  send_midi_stop();
#if ZV_ENABLED
  assert(stream_writes == before); // Timer calls never enter TinyUSB.
  zv_realtime_service(true, realtime_sink);
  assert(realtime_written == 3);
#else
  assert(stream_writes == before + 3); // Other devices preserve their behavior.
#endif
}
int main(void)
{
  assert(MIDI_NOTE_KEY == EXPECT_NOTE_KEY);
  assert(ZV_ENABLED == ZEPTOCORE_VISUALIZER);
  ZV_CALL(visualizer_only_probe());
  test_notes();
  test_usb();
#ifdef INCLUDE_ZEPTOCORE
  test_receive_channels();
  test_commands();
#endif
  test_clock_output();
  printf("MIDI tests passed (MIDI_NOTE_KEY=%d)\n", MIDI_NOTE_KEY);
  return 0;
}
