// Copyright 2023 Zack Scholl.
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
char *fil_current_name;
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
uint8_t banks_with_samples[16];
uint8_t banks_with_samples_num = 0;

FRESULT fil_result;
struct repeating_timer timer;
bool phase_forward = 1;
bool sync_using_sdcard = false;

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
uint vols[2];

float vol3 = 0;
float envelope_pitch_val;
float envelope_wobble_val;
uint beat_current = 0;
uint beat_total = 0;
uint debounce_quantize = 0;
uint32_t bpm_timer_counter = 0;
uint8_t retrig_beat_num = 0;
uint16_t retrig_timer_reset = 96;
bool retrig_first = false;
bool retrig_ready = false;
float retrig_vol = 1.0;
float retrig_vol_step = 0;

// buttons
// mode toggles
//   mode  ==0  ==1
uint8_t mode_buttons16 = 2;
bool mode_mute = 0;
bool mode_play = 0;

SaveFile *sf;
#ifdef INCLUDE_ZEPTOCORE
LEDS *leds;
LEDText *ledtext;
#endif

bool toggle_chain_play = false;
bool toggle_chain_rec = false;

int16_t dub_step_break = -1;
uint16_t dub_step_divider = 0;
uint8_t dub_step_beat = 0;

bool gate_active = false;
bool gate_is_applied = false;
uint32_t gate_counter = 0;
uint32_t gate_threshold = 10;

// add variable to keep track of variation
uint8_t sel_variation = 0;
int8_t sel_variation_next = 0;

bool quadratic_resampling = false;

#ifdef INCLUDE_BASS
Bass *bass;
#endif

#ifdef INCLUDE_SINEBASS
SinOsc *sinosc[3];
uint8_t sinebass_update_note = 0;
uint8_t sinebass_update_counter = 31;
#endif

ResonantFilter *resFilter[2];
Gate *audio_gate;
Saturation *saturation;

bool sdcard_startup_is_starting = false;
bool audio_mute = false;
bool trigger_audio_mute = false;
bool button_mute = false;
bool trigger_button_mute = false;

// fx toggles
bool fx_active[16];
bool fx_toggle[16];  // 16 possible

// lfos
int32_t lfo_pan_val = 0;
// TODO: make the lfo pan step adjustable?
int32_t lfo_pan_step = Q16_16_2PI / (96 * 5);
int32_t lfo_tremelo_val = 0;
// TODO: make the lfo tremelo step adjustable?
int32_t lfo_tremelo_step = Q16_16_2PI / (96);

#define ENVELOPE_PITCH_THRESHOLD 0.01
bool fx_tape_stop_active = false;

uint16_t global_filter_index = resonantfilter_fc_max;

// pitches derived from supercollider
// a=(Tuning.et(24).ratios/2)++Tuning.et(24).ratios++[2];
// a.do({ arg v;
// 	v.postln;
// });
#define PITCH_VAL_MAX 49
uint8_t pitch_val_index = 24;
float pitch_vals[PITCH_VAL_MAX] = {
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
    2.0,
};

// (
// var divisor = 2048;
// var start = -96;
// var end = 0;
// var increment = 0.5;
// var numberOfSteps = (end - start) / increment;
// var array = Array.series(numberOfSteps+1, start, increment);
// array.postln;
// ("#define VOLUME_DIVISOR "++divisor.asInteger).postln;
// ("#define VOLUME_STEPS "++numberOfSteps.asInteger).postln;
// ("const int32_t volume_vals["++numberOfSteps.asInteger++"] = {").postln;
// array.do({ arg v,i;
//   (((200*(v.dbamp)*divisor).round.asInteger.asString)++",").postln;
// });
// "};".postln;
// )
#define VOLUME_DIVISOR 2048
#define VOLUME_STEPS 192
const int32_t volume_vals[193] = {
    6,      7,      7,      8,      8,      9,      9,      10,     10,
    11,     12,     12,     13,     14,     15,     15,     16,     17,
    18,     19,     21,     22,     23,     24,     26,     27,     29,
    31,     33,     34,     37,     39,     41,     43,     46,     49,
    52,     55,     58,     61,     65,     69,     73,     77,     82,
    87,     92,     97,     103,    109,    115,    122,    130,    137,
    145,    154,    163,    173,    183,    194,    205,    217,    230,
    244,    258,    274,    290,    307,    325,    345,    365,    387,
    410,    434,    460,    487,    516,    546,    579,    613,    649,
    688,    728,    772,    817,    866,    917,    971,    1029,   1090,
    1154,   1223,   1295,   1372,   1453,   1539,   1631,   1727,   1830,
    1938,   2053,   2175,   2303,   2440,   2584,   2738,   2900,   3072,
    3254,   3446,   3651,   3867,   4096,   4339,   4596,   4868,   5157,
    5462,   5786,   6129,   6492,   6876,   7284,   7715,   8173,   8657,
    9170,   9713,   10289,  10898,  11544,  12228,  12953,  13720,  14533,
    15394,  16306,  17273,  18296,  19380,  20529,  21745,  23034,  24398,
    25844,  27375,  28997,  30716,  32536,  34464,  36506,  38669,  40960,
    43387,  45958,  48681,  51566,  54621,  57858,  61286,  64917,  68764,
    72838,  77154,  81726,  86569,  91698,  97131,  102887, 108983, 115441,
    122281, 129527, 137202, 145332, 153943, 163065, 172727, 182962, 193803,
    205286, 217450, 230335, 243983, 258440, 273754, 289975, 307157, 325357,
    344635, 365056, 386687, 409600,
};

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

#ifdef INCLUDE_RGBLED
WS2812 *ws2812;
#endif

uint8_t key_jump_debounce = 0;
inline void do_update_phase_from_beat_current() {
  // printf("[do_update_phase_from_beat_current] beat_current: %d\n",
  // beat_current);
  uint16_t slice =
      beat_current %
      banks[sel_bank_cur]->sample[sel_sample_cur].snd[sel_variation]->slice_num;
  banks[sel_bank_cur]
      ->sample[sel_sample_cur]
      .snd[sel_variation]
      ->slice_current = slice;
  if (phase_forward) {
    phase_new = banks[sel_bank_cur]
                    ->sample[sel_sample_cur]
                    .snd[sel_variation]
                    ->slice_start[slice];
  } else {
    phase_new = banks[sel_bank_cur]
                    ->sample[sel_sample_cur]
                    .snd[sel_variation]
                    ->slice_stop[slice];
  }
  gate_counter = 0;
  phase_change = true;
  audio_mute = false;
  Gate_reset(audio_gate);
  // printf("do_update_phase_from_beat_current: %d\n", phase_new);
}
#endif
