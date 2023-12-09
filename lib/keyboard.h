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

void run_keyboard() {
  int c = getchar_timeout_us(0);
  if (c >= 0) {
    if (c == '-' && sf->vol) sf->vol--;
    if ((c == '=' || c == '+') && sf->vol < 256) sf->vol++;
    if (c == ']') {
      if (sf->bpm_tempo < 300) {
        sf->bpm_tempo += 5;
      }
      printf("\nbpm: %d\n\n", sf->bpm_tempo);
    }
    if (c == 'c') {
      SaveFile_PatternRandom(sf, &rng, 0, 3);
      SaveFile_PatternPrint(sf);
      sf->pattern_on = 1 - sf->pattern_on;
    }
    if (c == 'v') {
      printf("current beat: %d, phase_new: %d, cpu util: %d\n", beat_current,
             phase_new, cpu_utilization);
#ifdef INCLUDE_BASS
      Bass_trig(bass, 0);
#endif
    }
    if (c == '[') {
      if (sf->bpm_tempo > 30) {
        sf->bpm_tempo -= 5;
      }
      printf("\nbpm: %d\n\n", sf->bpm_tempo);
    }
    if (c == 'p') {
      phase_forward = !phase_forward;
    }
    if (c == 'w') {
      phase_new = (phase_new / 4) * 4;
      phase_change = true;
      debounce_quantize = 2;
    }
    if (c == 'e') {
      phase_new = (phase_new / 4) * 4;
      phase_change = true;
      debounce_quantize = 2;
    }
    if (c == 'r') {
      retrig_first = true;
      retrig_beat_num = random_integer_in_range(8, 24);
      retrig_timer_reset =
          96 * random_integer_in_range(1, 4) / random_integer_in_range(2, 12);
      float total_time = (float)(retrig_beat_num * retrig_timer_reset * 60) /
                         (float)(96 * sf->bpm_tempo);
      retrig_vol_step = 1.0 / ((float)retrig_beat_num);
      printf("retrig_beat_num=%d,retrig_timer_reset=%d,total_time=%2.3f s\n",
             retrig_beat_num, retrig_timer_reset, total_time);
      retrig_ready = true;
    }
    if (c == 't') {
      phase_new = (phase_new / 4) * 4;
      phase_change = true;
      debounce_quantize = 2;
    }
    if (c == 'y') {
      phase_new = (phase_new / 4) * 4;
      phase_change = true;
      debounce_quantize = 2;
    }
    if (c == 'u') {
      phase_new = (phase_new / 4) * 4;
      phase_change = true;
      debounce_quantize = 2;
    }
    if (c == 'i') {
      phase_new = (phase_new / 4) * 4;
      phase_change = true;
      debounce_quantize = 2;
    }
    if (c == 'i') {
      phase_new = (phase_new / 4) * 4;
      phase_change = true;
      debounce_quantize = 2;
    }
    if (c == 's') {
      phase_new = (phase_new / 4) * 4;
      phase_change = true;
      debounce_quantize = 2;
    }
    if (c == 'd') {
      phase_new = (phase_new / 4) * 4;
      phase_change = true;
      debounce_quantize = 2;
    }
    if (c == 'f') {
      debounce_quantize = 2;
      beat_current = 10;
      phase_new = (phase_new / 4) * 4;
      phase_change = true;
    }
    if (c == 'g') {
      debounce_quantize = 2;
      beat_current = 11;
      phase_new = (phase_new / 4) * 4;
      phase_change = true;
    }
    if (c == 'h') {
      debounce_quantize = 2;
      beat_current = 12;
      phase_new = (phase_new / 4) * 4;
      phase_change = true;
    }
    if (c == 'j') {
      debounce_quantize = 2;
      beat_current = 13;
      phase_new = (phase_new / 4) * 4;
      phase_change = true;
    }
    if (c == 'k') {
      debounce_quantize = 2;
      beat_current = 14;
      phase_new = (phase_new / 4) * 4;
      phase_change = true;
    }
    if (c == 'l') {
      debounce_quantize = 2;
      beat_current = 15;
      phase_new = (phase_new / 4) * 4;
      phase_change = true;
    }
    if (c == 'b') {
      run_mount();
    }
    if (c == 'n') {
      // big_file_test("testfile", 1, 1);
      SaveFile_Load(sf);
    }
    if (c == 'm') {
      SaveFile_Save(sf, &sync_using_sdcard);
    }
    if (c == '9') {
      sf->saturate_wet++;
      printf("saturate_wet: %d\n", sf->saturate_wet);
    }
    if (c == '8') {
      if (sf->saturate_wet > 0) {
        sf->saturate_wet--;
      }
      printf("saturate_wet: %d\n", sf->saturate_wet);
    }
    if (c == '.') {
      sf->distortion_level++;
      printf("distortion_level: %d\n", sf->distortion_level);
    }
    if (c == ',') {
      if (sf->distortion_level > 0) {
        sf->distortion_level--;
      }
      printf("distortion_level: %d\n", sf->distortion_level);
    }
    if (c == '1') {
      sel_sample_next = 0;
      fil_current_change = true;
    }
    if (c == '2') {
      sel_sample_next = 1;
      fil_current_change = true;
    }
    if (c == '3') {
      sel_sample_next = 2;
      fil_current_change = true;
    }
    if (c == 'a') {
      Envelope2_reset(envelope_volume, BLOCKS_PER_SECOND, 0, 1.0, 1.5);
      // debounce_quantize = 2;
    }
    if (c == 'q') {
      Envelope2_reset(envelope_pitch, BLOCKS_PER_SECOND,
                      Envelope2_update(envelope_pitch), 1.0, 1);
      debounce_quantize = 2;
    }
    if (c == 'z') {
      debounce_quantize = 2;
      Envelope2_reset(envelope_pitch, BLOCKS_PER_SECOND,
                      Envelope2_update(envelope_pitch), 0.5, 1);
    }
    if (c == 'x') {
      sdcard_startup();
      // for (uint16_t i = 0; i < 2202; i++) {
      //   printf("%d\n", sf->vol - crossfade_vol(sf->vol, i));
      // }
    }
    // printf("sf->vol = %d      \r", sf->vol);
  }
}