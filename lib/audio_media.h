// Copyright 2026 Zack Scholl, GPLv3.0
// Common playback-handle lifecycle. Include after globals.h.
#ifndef AUDIO_MEDIA_H
#define AUDIO_MEDIA_H
#include <stdatomic.h>
static _Atomic uint32_t audio_media_recovery;
static _Atomic uint32_t audio_file_generation;
static FRESULT audio_file_last_result;
static void audio_media_io_failed(FRESULT result);

#if AUDIO_PREPARE_NEXT
static FIL audio_prepared_file;
static char audio_prepared_name[32];
static unsigned audio_prepared_stage;
static bool audio_prepared_adopting;
static volatile uint32_t audio_prepare_opens, audio_prepare_adopts, audio_prepare_warms;
static void audio_prepare_cancel(void) {
    seek_maps_detach(&audio_prepared_file);
    if(audio_prepared_file.obj.fs)audio_media_io_failed(f_close(&audio_prepared_file));
    memset(&audio_prepared_file,0,sizeof audio_prepared_file);
    audio_prepared_stage=0;audio_prepared_name[0]=0;
}
// One preparation step, under the audio core's existing filesystem ownership.
static void audio_prepare_step(const char *path) {
    if(strcmp(path,audio_prepared_name))audio_prepare_cancel();
    if(!audio_prepared_stage) {
        snprintf(audio_prepared_name,sizeof audio_prepared_name,"%s",path);
        FRESULT r=zd_f_open(&audio_prepared_file,path,FA_READ);
        if(r!=FR_OK) {audio_media_io_failed(r);audio_prepared_stage=3;return;}
        ++audio_prepare_opens;audio_prepared_stage=1;
    } else if(audio_prepared_stage==1) {
        seek_maps_load_existing(path);
        if(!seek_maps_media_ready())audio_media_io_failed(FR_DISK_ERR);
        seek_maps_attach(&audio_prepared_file,path);
        audio_prepared_stage=2;
    }
}
static bool audio_prepare_ready(const char *path) {
    return audio_prepared_stage>=2&&!strcmp(path,audio_prepared_name);
}
static void audio_prepare_warm(FSIZE_t offset) {
    if(audio_prepared_stage!=2||offset>=f_size(&audio_prepared_file))return;
    // With FF_FS_TINY=0 a one-byte read warms the file's own 512-byte sector.
    // Rewind within that sector so the real renderer can consume it normally.
    uint8_t byte;UINT count;
    FRESULT r=f_lseek(&audio_prepared_file,offset);
    if(r==FR_OK)r=f_read(&audio_prepared_file,&byte,1,&count);
    if(r==FR_OK&&count==1) {
        r=f_lseek(&audio_prepared_file,offset);
        if(r==FR_OK)++audio_prepare_warms;
    }
    audio_media_io_failed(r);
}
#endif

static void audio_media_io_failed(FRESULT result) {
    if(result==FR_DISK_ERR||result==FR_INT_ERR||result==FR_NOT_READY)
        atomic_store_explicit(&audio_media_recovery,result,memory_order_release);
}
static FRESULT audio_file_close(void) {
#if AUDIO_PREPARE_NEXT
    if(!audio_prepared_adopting)audio_prepare_cancel();
#endif
    // The caller owns the filesystem: either the playback callback, or core 0
    // after acknowledgement. Clear publication before detaching the old map.
    fil_is_open=false;
#if defined(SEEK_DIAGNOSTICS) && SEEK_DIAGNOSTICS
    zd_switch_file(UINT32_MAX);
#endif
    seek_maps_detach(&fil_current);
    FRESULT result=FR_OK;
    if(fil_current.obj.fs)result=zd_f_close(&fil_current);
    memset(&fil_current,0,sizeof fil_current);
    audio_media_io_failed(result);return result;
}
static FRESULT audio_file_open(const char *path) {
#if AUDIO_PREPARE_NEXT
    audio_prepared_adopting=audio_prepared_stage==2&&audio_prepare_ready(path);
#endif
    FRESULT result=audio_file_close();
#if AUDIO_PREPARE_NEXT
    bool adopt=audio_prepared_adopting;audio_prepared_adopting=false;
    if(result!=FR_OK)audio_prepare_cancel();
#endif
    if(result!=FR_OK) {audio_file_last_result=result;return result;}
    // path can be fil_current_name itself.
    if(path!=fil_current_name) {
        size_t length=strlen(path);
        if(length>=sizeof fil_current_name)return FR_INVALID_NAME;
        memmove(fil_current_name,path,length+1);
    }
#if AUDIO_PREPARE_NEXT
    if(adopt) {
        fil_current=audio_prepared_file;
        memset(&audio_prepared_file,0,sizeof audio_prepared_file);
        audio_prepared_stage=0;audio_prepared_name[0]=0;
        ++audio_prepare_adopts;result=FR_OK;
    } else
#endif
    result=zd_f_open(&fil_current,fil_current_name,FA_READ);
    audio_file_last_result=result;last_seeked=UINT32_MAX;
    if(result==FR_OK) {
        if(!fil_current.cltbl)seek_maps_attach(&fil_current,fil_current_name);
#if defined(SEEK_DIAGNOSTICS) && SEEK_DIAGNOSTICS
        uint16_t trace_id;
        if(seek_maps_file_id(fil_current_name,&trace_id))zd_switch_file(trace_id);
#endif
        audio_file_generation=audio_file_generation+1;if(!audio_file_generation)audio_file_generation=1;
        fil_is_open=true;
    } else {
        memset(&fil_current,0,sizeof fil_current);audio_media_io_failed(result);
    }
    return result;
}
// The normal startup lifecycle handles recovery from uncertain media. A full
// restart reloads .info/DSP state too, which an in-place FIL reopen cannot do for
// a replacement card. Called only from the foreground loop.
static void audio_media_recovery_poll(void) {
    if(atomic_load_explicit(&audio_media_recovery,memory_order_acquire))
        watchdog_reboot(0,SRAM_END,0);
}
#endif
