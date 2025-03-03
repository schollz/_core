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

#define WAV_HEADER 44
#define SAMPLE_RATE 44100

#define US_PER_BLOCK 1000000 * SAMPLES_PER_BUFFER / SAMPLE_RATE

audio_buffer_pool_t *init_audio() {
  static audio_format_t audio_format = {.pcm_format = AUDIO_PCM_FORMAT_S16,
                                        .sample_freq = SAMPLE_RATE,
                                        .channel_count = 2};

  static audio_buffer_format_t producer_format = {.format = &audio_format,
                                                  .sample_stride = 4};

  audio_buffer_pool_t *producer_pool =
      audio_new_producer_pool(&producer_format, 3,
                              SAMPLES_PER_BUFFER);  // todo correct size
  bool __unused ok;
  const audio_format_t *output_format;
  audio_i2s_config_t config = {.data_pin = PICO_AUDIO_I2S_DATA_PIN,
                               .clock_pin_base = PICO_AUDIO_I2S_CLOCK_PIN_BASE,
                               .dma_channel = 0,
                               .pio_sm = 0};

  output_format = audio_i2s_setup(&audio_format, &audio_format, &config);
  if (!output_format) {
    panic("PicoAudio: Unable to open audio device.\n");
  }

  ok = audio_i2s_connect(producer_pool);
  assert(ok);
  {  // initial buffer data
    audio_buffer_t *buffer = take_audio_buffer(producer_pool, true);
    int16_t *samples = (int16_t *)buffer->buffer->bytes;
    for (uint i = 0; i < buffer->max_sample_count; i++) {
      samples[i * 2 + 0] = 0;
      samples[i * 2 + 1] = 0;
    }
    buffer->sample_count = buffer->max_sample_count;
    give_audio_buffer(producer_pool, buffer);
  }
  audio_i2s_set_enabled(true);
  return producer_pool;
}

static inline uint32_t _millis(void) {
  return to_ms_since_boot(get_absolute_time());
}

clock_t clock() { return (clock_t)time_us_64() / 10000; }
