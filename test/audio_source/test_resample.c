#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "audio_resample.h"

static void legacy(int16_t *out, const int16_t *in, uint32_t n, uint32_t frames) {
  uint32_t step = n * 512 / frames;
  for (uint32_t i = 0; i < frames; ++i) {
    uint32_t fixed = i * step, index = fixed / 512, frac = fixed % 512;
    int32_t x = (int32_t)in[index] * (512 - frac) + (int32_t)in[index+1] * frac;
    out[i] = x / 512;
  }
}
int main(void) {
  uint32_t seed = 12345, cases = 0;
  for (unsigned frames = 256; frames <= 441; frames += 185) {
    unsigned capacity = (frames * 4 + 4) * 2;
    for (unsigned channels = 1; channels <= 2; ++channels) {
      assert(audio_source_frame_limit(UINT32_MAX, capacity, channels) == capacity/channels-1);
      for (unsigned requested = 11; requested <= capacity + 10; requested += 7) {
        unsigned n = audio_source_frame_limit(requested, capacity, channels);
        assert((n+1)*channels <= capacity);
        int16_t *source = malloc((n+1)*channels*sizeof *source);
        int16_t *copy = malloc((n+1)*channels*sizeof *source);
        int16_t *channel = malloc((n+1)*sizeof *channel);
        int16_t old[441], actual[441];
        for (unsigned i=0;i<(n+1)*channels;++i) {
          seed = seed*1664525u+1013904223u; source[i]=(int16_t)(seed>>16);
        }
        memcpy(copy,source,(n+1)*channels*sizeof *source);
        for (unsigned c=0;c<channels;++c) {
          for(unsigned i=0;i<=n;++i)channel[i]=source[i*channels+c];
          legacy(old,channel,n,frames);
          audio_resample_linear(actual,source+c,n,frames,channels);
          assert(!memcmp(old,actual,frames*sizeof *old)); ++cases;
        }
        assert(!memcmp(copy,source,(n+1)*channels*sizeof *source));
        free(source);free(copy);free(channel);
      }
    }
  }
  printf("%u mono/stereo comparisons: identical PCM, source unchanged, bounded lookahead\n",cases);
}
