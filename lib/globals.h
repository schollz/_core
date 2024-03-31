// Copyright 2023-2024 Zack Scholl.
//
// Author: Zack Scholl (zack.scholl@gmail.com)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// See http://creativecommons.org/licenses/MIT/ for more information.

#ifndef GLOBALS_LIB
#define GLOBALS_LIB 1

// audio_pool.h
audio_buffer_pool_t *ap;

clock_t time_of_initialization;
FIL fil_current;
char fil_current_name[32];
bool fil_is_open;
uint8_t cpu_utilization;
int32_t phases[2];
int32_t phases_old[2];
int32_t phase_new;
uint16_t mem_index[2];
bool mem_use;
bool phase_change;
unsigned int fil_bytes_read;
unsigned int fil_bytes_read2;
// uint16_t sf->bpm_tempo = 185;
uint16_t bpm_last = 185;
uint8_t sel_sample_cur = 0;
uint8_t sel_sample_next = 0;
uint8_t sel_bank_cur = 0;
uint8_t sel_bank_next = 0;
uint8_t sel_bank_select = 0;
bool fil_current_change = false;
SampleList *banks[16];
uint8_t banks_with_samples[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
uint8_t banks_with_samples_num = 0;

FRESULT fil_result;
struct repeating_timer timer;
bool phase_forward = 1;
bool sync_using_sdcard = false;

SequencerHandler sequencerhandler[3];

// voice 1 + 2
// voice 1 is always an envelope UP
// voice 2 is always an envelope DOWN
// voice 1 is only voice that jumps
// voice 2 takes place of old voice and continues
Envelope2 *envelope_volume;
Envelope2 *envelope_pitch;
EnvelopeLinearInteger *envelope_filter;
Noise *noise_wobble;
BeatRepeat *beatrepeat;
Delay *delay;
Comb *combfilter;
uint vols[2];

float vol3 = 0;
float envelope_pitch_val;
float envelope_wobble_val;
int32_t beat_current = 0;
int32_t beat_total = 0;
uint debounce_quantize = 0;
int64_t bpm_timer_counter = 0;
int64_t bpm_timer_counter_last = 0;
uint8_t retrig_beat_num = 0;
uint16_t retrig_timer_reset = 96;
bool retrig_first = false;
bool retrig_ready = false;
float retrig_vol = 1.0;
float retrig_vol_step = 0;
uint8_t retrig_pitch = 48;
int8_t retrig_pitch_change = 0;
int32_t scratch_lfo_inc = 0;
int32_t scratch_lfo_val = 0;
float scratch_lfo_hz = 0.7;
#define SCRATCH_LFO_1_HZ 9340

bool only_play_kicks = false;
bool only_play_snares = false;

// buttons
// mode toggles
//   mode  ==0  ==1
uint8_t mode_buttons16 = 0;
bool mode_mute = 0;
bool mode_play = 0;
bool mute_because_of_playback_type = false;
bool mode_hands_on_unmute = false;

bool key3_activated = false;
uint8_t key3_pressed_keys[3] = {0, 0, 0};

SaveFile *sf;
#ifdef INCLUDE_ZEPTOCORE
LEDS *leds;
LEDText *ledtext;
#endif

#ifdef INCLUDE_MIDI
#define MIDIOUTS 6
MidiOut *midiout[6];
#endif

bool toggle_chain_play = false;
bool toggle_chain_rec = false;

int16_t dub_step_break = -1;
uint16_t dub_step_divider = 0;
uint8_t dub_step_beat = 0;

uint32_t gate_threshold = 10;

// add variable to keep track of variation
uint8_t sel_variation = 0;
int8_t sel_variation_next = 0;
bool sel_variation_fadeout = false;

bool quadratic_resampling = false;
bool clock_out_do = false;
bool clock_out_ready = false;
bool clock_in_do = false;
bool clock_in_ready = false;
uint8_t clock_in_activator = 0;
int32_t clock_in_beat_total = 0;
uint32_t clock_in_diff_2x = 0;
uint32_t clock_in_last_time = 0;

uint8_t do_update_beat_repeat = 0;

uint8_t savefile_current = 0;
bool savefile_has_data[16];

bool do_stop_playback = false;
bool do_restart_playback = true;
bool playback_stopped = true;
bool playback_restarted = false;
bool audio_callback_in_mute = false;

#ifdef INCLUDE_BASS
Bass *bass;
#endif

#ifdef INCLUDE_SINEBASS
WaveBass *wavebass;
#endif

ResonantFilter *resFilter[2];
Gate *audio_gate;
TapTempo *taptempo;
Saturation *saturation;

#define DEBOUNCE_UINT8_LED_BAR 0
#define DEBOUNCE_UINT8_LED_SPIRAL1 1
#define DEBOUNCE_UINT8_LED_WALL 2
#define DEBOUNCE_UINT8_LED_DIAGONAL 3
#define DEBOUNCE_UINT8_LED_RANDOM1 4
#define DEBOUNCE_UINT8_LED_RANDOM2 5
#define DEBOUNCE_UINT8_LED_TRIANGLE 6
#define DEBOUNCE_UINT8_NUM 7
DebounceUint8 *debouncer_uint8[DEBOUNCE_UINT8_NUM];
#ifdef INCLUDE_ZEPTOCORE
DebounceDigits *debouncer_digits;
#endif

MessageSync *messagesync;
bool sdcard_startup_is_starting = false;
bool button_mute = false;
bool trigger_button_mute = false;

// reverb
FV_Reverb *freeverb;
bool freeverb_ready = false;
bool freeverb_inuse = false;

// lfos
int32_t lfo_pan_val = 0;
// TODO: make the lfo pan step adjustable?
int32_t lfo_pan_step = Q16_16_2PI / (96 * 5);
int32_t lfo_tremelo_val = 0;
// TODO: make the lfo tremelo step adjustable?
int32_t lfo_tremelo_step = Q16_16_2PI / (96);

#define ENVELOPE_PITCH_THRESHOLD 0.01

uint16_t global_filter_index = resonantfilter_fc_max;

// pitches derived from supercollider
/*
a=(Tuning.et(24).ratios/4)++(Tuning.et(24).ratios/2)++Tuning.et(24).ratios++[2];
a.do({ arg v;
        v.postln;
});
a.size
*/

// probability_max_values dictates how many out of every note will be activated
bool clock_did_activate = false;
const uint8_t probability_max_values[16] = {
    // 0 = never
    // 1 = 1/64
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 24, 32, 48,
};
const uint8_t probability_max_values_off[16] = {
    // 0 = never
    // 1 = 1/64
    0, 48, 32, 28, 26, 24, 22, 20, 18, 16, 14, 12, 10, 8, 6, 4,
};
#define PITCH_VAL_MAX 73
#define PITCH_VAL_MID 48
float pitch_vals[PITCH_VAL_MAX] = {
    0.25,
    0.25732555916084,
    0.26486577358976,
    0.27262693316622,
    0.28061551207721,
    0.2888381742179,
    0.29730177875047,
    0.30601338582592,
    0.31498026247343,
    0.32420988866242,
    0.33370996354212,
    0.34348841186409,
    0.35355339059278,
    0.36391329571,
    0.37457676921856,
    0.38555270635132,
    0.39685026299132,
    0.40847886330947,
    0.42044820762598,
    0.43276828050212,
    0.44544935906914,
    0.45850202160122,
    0.47193715633965,
    0.48576597057551,
    0.5,
    0.51465111832169,
    0.52973154717953,
    0.54525386633244,
    0.56123102415443,
    0.5776763484358,
    0.59460355750095,
    0.61202677165183,
    0.62996052494685,
    0.64841977732483,
    0.66741992708425,
    0.68697682372817,
    0.70710678118557,
    0.72782659142,
    0.74915353843713,
    0.77110541270263,
    0.79370052598263,
    0.81695772661895,
    0.84089641525197,
    0.86553656100424,
    0.89089871813828,
    0.91700404320245,
    0.94387431267929,
    0.97153194115102,
    1.0,
    1.0293022366434,
    1.0594630943591,
    1.0905077326649,
    1.1224620483089,
    1.1553526968716,
    1.1892071150019,
    1.2240535433037,
    1.2599210498937,
    1.2968395546497,
    1.3348398541685,
    1.3739536474563,
    1.4142135623711,
    1.45565318284,
    1.4983070768743,
    1.5422108254053,
    1.5874010519653,
    1.6339154532379,
    1.6817928305039,
    1.7310731220085,
    1.7817974362766,
    1.8340080864049,
    1.8877486253586,
    1.943063882302,
    2,
};

#define MODE_JUMP 0
#define MODE_MASH 1
#define MODE_BASS 2

// ignore boundaries
#define PLAY_NORMAL 0
// starts at splice start and ends at splice stop
#define PLAY_SPLICE_STOP 1
// starts at splice start, and returns to start when reaching splice boundary
#define PLAY_SPLICE_LOOP 2
// starts at splice start and ends at sample boundary
#define PLAY_SAMPLE_STOP 3
// starts at splice start and returns to start when reaching sample boundary
#define PLAY_SAMPLE_LOOP 4

#ifdef INCLUDE_ECTOCORE
#define TRIGGER_MODE_KICK 0
#define TRIGGER_MODE_SNARE 1
#define TRIGGER_MODE_HH 2
#define TRIGGER_MODE_RANDOM 3
uint8_t ectocore_trigger_mode = 0;
uint8_t cv_signals[3] = {MCP_CV_AMEN, MCP_CV_BREAK, MCP_CV_SAMPLE};
uint8_t cv_attenuate[2] = {MCP_ATTEN_AMEN, MCP_ATTEN_BREAK};
#define CV_AMEN 0
#define CV_BREAK 1
#define CV_SAMPLE 2
int16_t cv_values[3] = {0, 0, 0};
bool cv_plugged[3] = {false, false, false};
int8_t cv_beat_current_override = -1;

#define ECTOCORE_CLOCK_NUM_DIVISIONS 7
const uint8_t ectocore_clock_out_divisions[ECTOCORE_CLOCK_NUM_DIVISIONS] = {
    8, 8 * 2, 8 / 2, 8 * 4, 8 / 4, 8 * 8, 8 / 8,
};
uint8_t ectocore_clock_selected_division = 0;
#endif

#ifdef INCLUDE_RGBLED
WS2812 *ws2812;
#endif

uint8_t key_jump_debounce = 0;
bool do_random_jump = false;

void do_update_phase_from_beat_current() {
  // printf("[do_update_phase_from_beat_current] beat_current: %d\n",
  //        beat_current);
  // printf_sysex("[global] beat_current: %d\n", beat_current);
  if (do_random_jump) {
    beat_current = random_integer_in_range(0, 15);
    do_random_jump = false;
  }
  uint16_t slice =
      beat_current %
      banks[sel_bank_cur]->sample[sel_sample_cur].snd[FILEZERO]->slice_num;
  banks[sel_bank_cur]->sample[sel_sample_cur].snd[FILEZERO]->slice_current =
      slice;
  if (phase_forward) {
    phase_new = banks[sel_bank_cur]
                    ->sample[sel_sample_cur]
                    .snd[FILEZERO]
                    ->slice_start[slice] *
                sel_variation_scale[sel_variation];
  } else {
    phase_new = banks[sel_bank_cur]
                    ->sample[sel_sample_cur]
                    .snd[FILEZERO]
                    ->slice_stop[slice] *
                sel_variation_scale[sel_variation];
  }
  mute_because_of_playback_type = false;
  phase_change = true;
  Gate_reset(audio_gate);
#ifdef INCLUDE_ECTOCORE
  gpio_put(GPIO_LED_TAPTEMPO, slice % 2 == 0);
#endif

  // printf("[globals] do_update_phase_from_beat_current: %d, %d %d [%d, %d]\n",
  //        beat_current, slice, phase_new,
  //        banks[sel_bank_cur]
  //            ->sample[sel_sample_cur]
  //            .snd[FILEZERO]
  //            ->slice_start[slice],
  //        banks[sel_bank_cur]
  //            ->sample[sel_sample_cur]
  //            .snd[FILEZERO]
  //            ->slice_stop[slice]);
}

void key_do_jump(uint8_t beat) {
  if (beat >= 0 && beat < 16) {
    printf("key_do_jump %d\n", beat);
    // TODO: [0] should be which sequencer it is on
    if (sequencerhandler[0].recording) {
      Sequencer_add(sf->sequencers[0][sf->sequence_sel[0]], beat,
                    bpm_timer_counter);
    }
    key_jump_debounce = 1;
    beat_current = floor(beat_current / 16) * 16 + beat;
    retrig_pitch = PITCH_VAL_MID;
    do_update_phase_from_beat_current();
  }
}

void step_sequencer_emit(uint8_t key) {
#ifdef INCLUDE_MIDI
  // midi out
  MidiOut_on(midiout[3], key, 127);
#endif
  key_do_jump(key);
}
void step_sequencer_stop() { printf("stop\n"); }

#endif
