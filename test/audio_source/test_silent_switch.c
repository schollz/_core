#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef int FRESULT;
#define FR_OK 0
static bool sync_using_sdcard, fil_current_change, fil_current_change_force;
static bool do_open_file_ready, phase_change, mute_because_of_playback_type;
static bool button_mute = true, playback_stopped = true;
static uint8_t sel_bank_cur, sel_bank_next, sel_sample_cur, sel_sample_next;
static uint8_t sel_variation, audio_variant;
static int phases[2], phase_new;
static char fil_current_name[32];
static struct Bank { unsigned num_samples; } storage[16], *banks[16];
static unsigned opens, resets;
static FRESULT open_result;
static void format_sample_filename(char *p, unsigned b, unsigned s, unsigned v) {
  snprintf(p, 32, "bank%u/%u.%u.wav", b+1, s, v);
}
static FRESULT audio_file_open(const char *p) {
  assert(sync_using_sdcard); assert(!strcmp(p, "bank3/1.0.wav"));
  ++opens; return open_result;
}
static void realtime_stretch_reset_from_playback_phase(void) { ++resets; }
#include "audio_silent_switch.h"
int main(void) {
  storage[2].num_samples=3; banks[2]=&storage[2];
  sel_bank_cur=1; sel_bank_next=2; sel_sample_next=4;
  fil_current_change=true; mute_because_of_playback_type=true;
  phases[0]=900; phases[1]=800; phase_new=700;
  audio_switch_while_silent();
  assert(opens==1 && resets==1 && sel_bank_cur==2 && sel_sample_cur==1);
  assert(!fil_current_change && !mute_because_of_playback_type && phase_change);
  assert(!phases[0] && !phases[1] && !phase_new && !sync_using_sdcard);
  assert(button_mute && playback_stopped);
  audio_switch_while_silent(); assert(opens==1);
  fil_current_change=true; audio_switch_while_silent();
  assert(!fil_current_change && opens==1);
  fil_current_change_force=true; sync_using_sdcard=true;
  audio_switch_while_silent(); assert(opens==1 && fil_current_change_force);
  sync_using_sdcard=false; open_result=4; mute_because_of_playback_type=true;
  audio_switch_while_silent();
  assert(opens==2 && resets==1 && !fil_current_change_force);
  assert(mute_because_of_playback_type && !sync_using_sdcard);
  open_result=0; do_open_file_ready=true;
  audio_switch_while_silent(); assert(opens==3 && resets==2 && !do_open_file_ready);
  sel_bank_next=16; fil_current_change=true;
  audio_switch_while_silent(); assert(opens==3 && fil_current_change);
  sel_bank_next=3; audio_switch_while_silent(); assert(opens==3);
  puts("silent sample switch regression checks passed");
}
