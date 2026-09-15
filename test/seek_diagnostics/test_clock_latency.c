#include "clock_latency.h"
#include <assert.h>
#include <stdio.h>
uint32_t zd_test_time;
uint32_t cl_starvation(void) { return 0; }
int main(void) {
  zd_test_time = 100; cl_begin(2, 0, 1);
  zd_test_time = 105; cl_phase(0, 4096);
  zd_test_time = 110; cl_handler_done();
  assert(cl_render_tag() == 0);
  zd_test_time = 120; cl_applied(4096);
  uint32_t tag = cl_render_tag();
  assert(tag == (CL_TAG | 512));
  int16_t pcm[512] = {0}; pcm[10] = 50;
  zd_test_time = 200; cl_seek(0, 256, 4, 16); cl_submit(tag, pcm, 256);
  assert(cl_copy(0, tag, 16) == (tag | 16));
  assert(cl_copy(tag | 16, tag, 30) == (tag | 16));
  cl_dma(tag | 16, 6000);
  assert(clock_latency_records[1].dma_us == 6000);
  assert(clock_latency_records[1].first_nonzero == 5);
  assert(clock_latency_records[1].phase_us == 105);
  assert(clock_latency_records[1].applied_us == 120);
  assert(clock_latency_records[1].dma_offset == 16);
  assert(cl_render_tag() == 0);
  zd_test_time = 10000; cl_begin(7, 0, 2);
  cl_dma(tag, 11000); // stale token cannot complete a new request
  cl_timeout();
  assert(clock_latency_records[2].flags & (1u << 31));
  assert(clock_latency_records[2].dma_us == 0);
  for (unsigned i = 0; i < 8; ++i)
    assert(!(clock_latency_records[i].sequence & 1));
  puts("clock latency: stage attribution, PCM onset, DMA offset, stale tags and timeout pass");
}
