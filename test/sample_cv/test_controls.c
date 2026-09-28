// Exercise extracted production sample CV, knob, and bank-selection branches.
#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "sample_cv.h"
#include "utils.h"

#define CV_SAMPLE 2
#define GPIO_BTN_BANK 20
#define MODE_HOLD_DURATION_THRESHOLD 1000
#define DEBOUNCE_FILE_SWITCH 500
#define FX_TIGHTEN 0

static SampleCVState sample_cv_state;
static SampleCVMapping global_sample_cv_mapping;
static bool global_sample_cv_bipolar = true, cv_plugged[3];
static int cv_reset_override = -1;
typedef struct {
  uint16_t num_samples;
} TestBank;
static TestBank bank_storage[2] = {{8}, {5}};
static TestBank *banks[] = {&bank_storage[0], &bank_storage[1]};
static uint8_t banks_with_samples[] = {0, 1}, banks_with_samples_num = 2;
static uint8_t sel_bank_cur, sel_bank_next_new, sel_sample_cur,
    sel_sample_next_new;
static int debounce_file_change, mode_held_duration, mode_chaos_trembler;
static bool dont_wait, fil_current_change, sync_using_sdcard, bank_held;
static int gpio_btn_taptempo_val = 1;
typedef struct {
  bool fx_active[1];
  int fx_param[1][1];
} TestSavefile;
static TestSavefile savefile;
static TestSavefile *sf = &savefile;
static unsigned gate_changes;
static void *audio_gate;

static bool gpio_get(int gpio) {
  assert(gpio == GPIO_BTN_BANK);
  return !bank_held;
}
static void toggle_fx(int effect) {
  sf->fx_active[effect] = !sf->fx_active[effect];
}
static void Gate_set_amount(void *gate, int amount) {
  (void)gate;
  (void)amount;
  ++gate_changes;
}
#define ws2812_set_wheel(...) ((void)0)
#define ws2812_set_wheel_section(...) ((void)0)
#define ws2812_wheel_clear(...) ((void)0)
#define WS2812_fill_color(...) ((void)0)
#define WS2812_show(...) ((void)0)

static bool active(void) {
#include "active.h"
  return sample_cv_1voct_active;
}
static void cv(int raw_adc) {
  bool sample_cv_1voct_active = active();
  if (!cv_plugged[CV_SAMPLE] || cv_reset_override == CV_SAMPLE) return;
  int16_t val = raw_adc - 512;
  for (int once = 0; once < 1; ++once) {
#include "sample_control.h"
  }
}
static void knob(int16_t val) {
  bool sample_cv_1voct_active = active();
#include "sample_knob.h"
}
static void button_bank(void) {
  bool sample_cv_1voct_active = active();
  sel_bank_next_new = 1;
#include "button_bank.h"
}

int main(void) {
  cv_plugged[CV_SAMPLE] = true;
  // Default behavior is identical across every ADC value and supported bank
  // size.
  for (unsigned count = 1; count <= 16; ++count) {
    banks[0]->num_samples = count;
    for (int bipolar = 0; bipolar <= 1; ++bipolar) {
      global_sample_cv_bipolar = bipolar;
      for (int raw = 0; raw <= 1023; ++raw) {
        cv(raw);
        unsigned input = bipolar ? raw : (raw < 512 ? 0 : raw - 512);
        unsigned expected = input * count / (bipolar ? 1024 : 512);
        assert(sel_sample_next_new == expected);
      }
    }
  }
  banks[0]->num_samples = 8;
  global_sample_cv_mapping = SAMPLE_CV_MAPPING_1VOCT;
  global_sample_cv_bipolar = true;
  sel_sample_cur = 4;
  debounce_file_change = 0;
  dont_wait = false;
  cv(614);  // +1 V: note 12 wraps to index 4 in this bank.
  assert(sel_sample_next_new == 4 && debounce_file_change == 0 && !dont_wait);
  cv(683);  // Note 20 maps to the same sample; no retrigger.
  assert(sel_sample_next_new == 4 && debounce_file_change == 0 && !dont_wait);
  cv(521);
  assert(sel_sample_next_new == 1 && debounce_file_change == 1 && dont_wait);
  cv(503);
  assert(sel_sample_next_new == 7);
  global_sample_cv_bipolar = false;
  cv(503);
  assert(sel_sample_next_new == 0);

  // A held note uses the destination bank, including both bank controls.
  global_sample_cv_bipolar = true;
  cv(614);
  bank_held = true;
  knob(1023);
  assert(sel_bank_next_new == 1 && sel_sample_next_new == 2);
  cv(614);
  assert(sel_sample_next_new == 2);
  sel_bank_next_new = 0;
  button_bank();
  assert(sel_sample_next_new == 2);

  // Normal knob selection cannot override CV; modifier gestures still work.
  bank_held = false;
  knob(0);
  assert(sel_sample_next_new == 2);
  mode_held_duration = MODE_HOLD_DURATION_THRESHOLD + 1;
  knob(1023);
  assert(mode_chaos_trembler == 99 && sel_sample_next_new == 2);
  mode_held_duration = 0;
  gpio_btn_taptempo_val = 0;
  knob(512);
  assert(gate_changes == 1 && sel_sample_next_new == 2);
  gpio_btn_taptempo_val = 1;

  banks[1]->num_samples = 0;
  cv(521);
  assert(sel_sample_next_new == 2);  // Empty banks never divide or select.
  banks[1]->num_samples = 5;
  sel_bank_next_new = 0;
  cv_plugged[CV_SAMPLE] = false;
  knob(1023);
  assert(!sample_cv_state.valid && sel_sample_next_new == 7);
  cv_plugged[CV_SAMPLE] = true;
  cv(512);
  assert(sample_cv_state.valid && sel_sample_next_new == 0);

  cv_reset_override = CV_SAMPLE;
  cv(614);
  assert(!sample_cv_state.valid && sel_sample_next_new == 0);
  knob(1023);
  assert(sel_sample_next_new == 7);
  cv_reset_override = -1;
  cv(614);
  assert(sample_cv_state.valid && sel_sample_next_new == 4);
  global_sample_cv_mapping = SAMPLE_CV_MAPPING_BANK;
  cv(512);
  assert(!sample_cv_state.valid && sel_sample_next_new == 4);
  button_bank();
  assert(sel_sample_next_new == 3);  // Existing proportional bank selection.
  puts(
      "Production sample CV, bank transitions, knob gestures, and reset "
      "override passed");
}
