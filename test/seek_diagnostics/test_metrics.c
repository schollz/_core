#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "seek_diagnostics.h"
#include "hardware/structs/rosc.h"
uint32_t zd_test_time;
struct zd_test_rosc zd_test_rosc = {1};
void zd_application_layout_snapshot(volatile uint32_t *out,uint16_t id) {
  out[0]=0x314d4c43;out[2]=id;
}

int main(void) {
  zd_metric_t m = {0};
  uint32_t saturated = 0;
  for (unsigned i = 0; i < ZD_BINS; i++) {
    zd_metric_record(&m, zd_bin_upper_us[i], i == 2, 100, i == 3, &saturated);
    assert(m.bins[i] == 1);
  }
  assert(m.count == 16 && m.errors == 1 && m.short_reads == 1);
  assert(m.max_us == UINT32_MAX && m.total_hi > 0);
  assert(m.bytes_lo == 1600 && !saturated);
  m.count = UINT32_MAX;
  m.total_lo = m.total_hi = UINT32_MAX;
  zd_metric_record(&m, 1, 0, 0, false, &saturated);
  assert(saturated && m.count == UINT32_MAX && m.total_hi == UINT32_MAX);
  assert(m.total_lo == UINT32_MAX);

  zd_init(225000000, 441);
  zd_audio_clock(225000000, 79, 16);
  assert(zeptocore_diag.header[31] > 9900);
  zd_audio.metrics[ZD_SEEK].count = 42;
  // Uncommitted payload must not cause a publication.
  zeptocore_diag.request.command = ZD_SNAPSHOT;
  zd_service(ZD_AUDIO);
  assert(zeptocore_diag.audio.header.sequence == 0);
  zeptocore_diag.request.sequence = 1;
  zd_service(ZD_AUDIO);
  assert(zeptocore_diag.audio.header.sequence == 1);
  assert(zeptocore_diag.audio.data.metrics[ZD_SEEK].count == 42);
  zd_audio.metrics[ZD_SEEK].count = 43;
  zd_service(ZD_AUDIO);
  assert(zeptocore_diag.audio.data.metrics[ZD_SEEK].count == 42);
  assert(zeptocore_diag.control.header.sequence == 0);
  zd_service(ZD_CONTROL);
  zd_service(ZD_IRQ);
  assert(zeptocore_diag.control.header.sequence == 1);
  assert(zeptocore_diag.irq.header.sequence == 1);
  // Bad arguments and unknown commands never publish a valid data payload.
  zeptocore_diag.request.arguments[0] = 1;
  zeptocore_diag.request.sequence = 2;
  zd_service(ZD_AUDIO);
  assert(zeptocore_diag.audio.header.status == ZD_MALFORMED);
  assert(zeptocore_diag.audio.header.payload_bytes == 0);
  zeptocore_diag.request.arguments[0] = 0;
  zeptocore_diag.request.command = 99;
  zeptocore_diag.request.sequence = 3;
  zd_service(ZD_AUDIO);
  assert(zeptocore_diag.audio.header.status == ZD_UNSUPPORTED);
  zeptocore_diag.request.command = ZD_SNAPSHOT;
  zeptocore_diag.request.sequence = 4;
  zd_service(ZD_AUDIO);
  assert(zeptocore_diag.audio.data.metrics[ZD_SEEK].count == 43);

  // Timer wrap is unsigned subtraction, not a negative or truncated duration.
  zd_test_time = 3;
  zd_record_io(ZD_READ, UINT32_MAX-5, 1, 3, true);
  assert(zd_audio.metrics[ZD_READ].total_lo == 9);
  assert(zd_audio.metrics[ZD_READ].errors == 1);
  assert(zd_audio.metrics[ZD_READ].short_reads == 1);
  zd_dma_start(true, 256);
  zeptocore_diag.header[21] = 3;
  zd_dma_start(true, 256);
  zeptocore_diag.request.sequence = 5;
  zd_service(ZD_IRQ);
  assert(zeptocore_diag.irq.data.counters[1] == 1);
  assert(zeptocore_diag.irq.data.counters[2] == 256);
  assert(zeptocore_diag.irq.data.counters[3] == 1);
  zeptocore_diag.request.command=ZD_MAP_LAYOUT;
  zeptocore_diag.request.arguments[0]=65535;
  zeptocore_diag.request.sequence=6;zd_service(ZD_CONTROL);
  assert(zeptocore_diag.control.header.status==ZD_OK);
  const volatile uint32_t *layout=(const volatile uint32_t *)&zeptocore_diag.control.data;
  assert(layout[0]==0x314d4c43&&layout[2]==65535&&layout[119]==0);
  zeptocore_diag.request.arguments[0]=65536;
  zeptocore_diag.request.sequence=7;zd_service(ZD_CONTROL);
  assert(zeptocore_diag.control.header.status==ZD_MALFORMED);
  puts("metrics/mailbox tests passed");
}
