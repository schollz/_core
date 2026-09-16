#ifndef CLOCK_LATENCY_H
#define CLOCK_LATENCY_H
#include <stdint.h>
#ifndef SEEK_CLOCK_LATENCY
#define SEEK_CLOCK_LATENCY 0
#endif
#if SEEK_CLOCK_LATENCY
#define CL_CALL(...) do { __VA_ARGS__; } while (0)
#ifdef __cplusplus
extern "C" {
#endif
#define CL_TAG 0x80000000u
typedef struct {
  uint32_t sequence, id, input_us, handler_done_us, phase_us, phase, beat;
  uint32_t applied_us, submit_us, dma_us, dma_offset, first_nonzero;
  uint32_t flags, negative_latency, source, mode, starvation_before, starvation_after;
  uint32_t source_frames, bytes_per_frame;
} clock_latency_record;
extern volatile clock_latency_record clock_latency_records[8];
void cl_begin(uint32_t flags, uint32_t source, uint32_t mode);
void cl_handler_done(void);
void cl_phase(uint32_t beat, uint32_t phase);
void cl_applied(uint32_t phase);
uint32_t cl_render_tag(void);
void cl_submit(uint32_t tag, const int16_t *pcm, uint32_t frames);
void cl_seek(int32_t offset, uint32_t frames, uint32_t bytes_per_frame, uint32_t flags);
uint32_t cl_copy(uint32_t destination, uint32_t source, uint32_t offset);
void cl_dma(uint32_t tag, uint32_t us);
void cl_timeout(void);
uint32_t cl_starvation(void);
#ifdef __cplusplus
}
#endif
#else
#define CL_CALL(...) ((void)0)
#endif
#endif
