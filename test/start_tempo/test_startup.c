#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include "start_tempo.h"

typedef struct { unsigned bpm_tempo, bank, sample; } SaveFile;
static SaveFile state, *sf;
static unsigned global_start_tempo, saved_tempo, expected_tempo, effect_calls;
static unsigned savefile_current, sel_bank_next, sel_sample_next;
static int deferred_preset_load;
static bool savefile_has_data[16], fil_current_change;
static char fil_current_name[32];
static struct {unsigned num_samples;} bank, *banks[16];
static bool bank_transition_busy(void) {return false;}
static bool audio_media_acquire(void) {return true;}
static void audio_media_release(void) {}
static bool SaveFile_load(SaveFile *s, unsigned slot) {(void)slot;s->bpm_tempo=saved_tempo;return true;}
static SaveFile *SaveFile_malloc(void) {state=(SaveFile){.bpm_tempo=170};return &state;}
static void audio_file_open(const char *name) {(void)name;}
static void update_fx(unsigned i) {(void)i;assert(sf->bpm_tempo==expected_tempo);++effect_calls;}
static int metadata_ordinal(unsigned b,unsigned s) {(void)b;return (int)s;}
#define Sequencer_set_callbacks(...) ((void)0)
#include "preset.h"

static void boot(unsigned configured, bool saved) {
  global_start_tempo=configured;savefile_has_data[0]=saved;
  saved_tempo=145;expected_tempo=configured?configured:(saved?145:170);effect_calls=0;
#include "boot.h"
  assert(savefile_load_state(false)==saved);
  assert(sf->bpm_tempo==expected_tempo);
  assert(effect_calls==(saved?16u:0u));
  // A runtime preset recall remains authoritative, even with a boot override.
  savefile_has_data[0]=true;expected_tempo=145;
  assert(savefile_load_state(true));assert(sf->bpm_tempo==145);
  assert(sel_bank_next==0 && sel_sample_next==0 && fil_current_change);
  assert(deferred_preset_load==0);
}
int main(void) {
  banks[0]=&bank;bank.num_samples=1;
  for(unsigned saved=0;saved<2;++saved) {
    boot(0,saved);boot(30,saved);boot(130,saved);boot(300,saved);
  }
  puts("Firmware startup: fixed tempo precedes effects; default and runtime presets preserved");
}
