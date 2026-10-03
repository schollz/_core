// Copyright 2026 Zack Scholl, GPLv3.0
// Common foreground operations, included after realtime_stretch.h.
#ifndef AUDIO_MEDIA_CONTROL_H
#define AUDIO_MEDIA_CONTROL_H
#include "bank_transition.h"
#if defined(SEEK_DIAGNOSTICS) && SEEK_DIAGNOSTICS
void __not_in_flash_func(zd_application_layout_snapshot)(volatile uint32_t *out,uint16_t id) {
    seek_maps_layout(id,out);
}
void __not_in_flash_func(zd_application_control_snapshot)(void) {
    uint32_t *c=zd_control.counters,*x=zd_control.context;
    // Heap capacity is below 256 KiB. Feature bit 32 identifies these packed
    // reverb counts, exposing changes in the existing memory-based DSP sizing.
    x[0]=(getTotalHeap()&0x00ffffffu)|
        (freeverb?((uint32_t)freeverb->num_combs<<24)|((uint32_t)freeverb->num_allpasses<<28):0);
    // Statistics do not publish file/cache ownership. Relaxed loads avoid a
    // barrier per counter; the immutable response still publishes as one unit.
#define MAP_STAT(field) atomic_load_explicit(&seek_maps_stats.field,memory_order_relaxed)
    c[0]=MAP_STAT(mount);c[1]=MAP_STAT(files);
    c[2]=MAP_STAT(builds);c[3]=MAP_STAT(reused);
    c[4]=MAP_STAT(validated);c[5]=MAP_STAT(invalid);
    c[6]=MAP_STAT(oversized);c[7]=MAP_STAT(loads);
    c[8]=MAP_STAT(evictions);c[11]=MAP_STAT(writes);
    c[12]=atomic_load_explicit(&audio_file_generation,memory_order_relaxed);c[13]=MAP_STAT(commits);
    c[14]=MAP_STAT(failures);c[15]=MAP_STAT(capacity);
    x[3]=MAP_STAT(validation_us);x[15]=MAP_STAT(build_us);
    x[21]=MAP_STAT(load_us);x[22]=MAP_STAT(commit_us);
    x[23]=MAP_STAT(prepare_us);x[24]=MAP_STAT(retained_bytes);
    x[25]=MAP_STAT(reserved_bytes);x[26]=MAP_STAT(generation);
    x[27]=MAP_STAT(pending);x[28]=MAP_STAT(last_error);
#undef MAP_STAT
    x[29]=audio_media_stats.acquisitions;x[30]=audio_media_stats.timeouts;
    x[31]=audio_media_stats.wait_max_us;
}
#endif
static void audio_media_poll(void) {
    bank_transition_service();
    if(bank_transition_busy())return;
    audio_media_recovery_poll();
    if(seek_maps_stats.pending&&playback_stopped&&audio_callback_in_mute&&
       audio_media_try_quiet()) {
        // Recheck transport after the boundary acknowledgement. These jobs
        // never create their own stop/mute or hold up a playing sample switch.
        if(playback_stopped) {
            seek_maps_service();
            if(!seek_maps_media_ready())audio_media_io_failed(FR_DISK_ERR);
            if(fil_is_open&&!fil_current.cltbl)
                seek_maps_attach(&fil_current,fil_current_name);
        }
        audio_media_release();
    }
}
#endif
