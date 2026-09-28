// Foreground only. Audio hands off a finished fade via bank_fade_done; the
// filesystem owner then acknowledges the last callback before arrays change.
#ifndef BANK_TRANSITION_IMPL_H
#define BANK_TRANSITION_IMPL_H
static struct {
    uint8_t bank, sample, previous_bank, previous_sample;
    uint8_t variation, variant;
    bool owned, rollback, previous_valid, silent, exhausted, changed_phase, replay;
    unsigned stage;
    uint32_t started, load_started;
    int32_t phase[2], next_phase, beat;
} bank_job;
static void bank_state(unsigned state) {
    metadata_status.state=state;
    atomic_store_explicit(&bank_transition_state,state,memory_order_release);
}
static void bank_finish(bool success) {
    uint32_t duration=time_us_32()-bank_job.started;
    metadata_status.transition_us=duration;
    if(duration>metadata_status.maximum_transition_us)metadata_status.maximum_transition_us=duration;
    audio_was_muted=true;
    if(success) {
        first_loop_ever=true;
        do_open_file_ready=false;
        realtime_stretch_reset_from_playback_phase();
    }
    // IO failures here are handled by the bounded rollback, not reboot/retry.
    atomic_store_explicit(&audio_media_recovery,0,memory_order_release);
    atomic_store_explicit(&bank_fade_done,false,memory_order_release);
    bank_state(BANK_IDLE);
    if(bank_job.replay)fil_current_change=true;
    if(bank_job.owned) {bank_job.owned=false;audio_media_release();}
}
static void bank_fail(void) {
    if(!bank_job.rollback && bank_job.previous_valid) {
        bank_job.rollback=true;bank_job.stage=0;
        ++metadata_status.rollbacks;bank_state(BANK_ROLLBACK);
    } else {bank_job.stage=0;bank_state(BANK_ERROR);}
}
static void bank_transition_service(void) {
    uint32_t free_heap=getFreeHeap();
    if(free_heap<metadata_status.minimum_free_heap)metadata_status.minimum_free_heap=free_heap;
    unsigned state=atomic_load_explicit(&bank_transition_state,memory_order_acquire);
    if(fil_current_change) {
        if(metadata_selection_valid(sel_bank_next,0))
            sel_sample_next%=banks[sel_bank_next]->num_samples;
        else fil_current_change=false;
    }
    bool request=fil_current_change && metadata_selection_valid(sel_bank_next,sel_sample_next);
    if(state==BANK_IDLE) {
        if(!request || (metadata_ready(sel_bank_next)&&fil_is_open))return;
        memset(&bank_job,0,sizeof bank_job);
        bank_job.bank=sel_bank_next;bank_job.sample=sel_sample_next;
        bank_job.started=time_us_32();bank_state(BANK_REQUESTED);
        fil_current_change=false;
        return;
    }
    // Accept the latest selection, including same-bank requests while loading.
    if(request) {
        fil_current_change=false;
        if(bank_job.rollback)bank_job.replay=true;
        bool changed=sel_bank_next!=bank_job.bank;
        bank_job.bank=sel_bank_next;bank_job.sample=sel_sample_next;
        if(changed && (state==BANK_LOADING||state==BANK_COMMITTING) && !bank_job.rollback) {
            bank_state(BANK_CANCEL);state=BANK_CANCEL;
        }
    }
    if(state==BANK_REQUESTED) {
        atomic_store_explicit(&bank_fade_silent,false,memory_order_relaxed);
        atomic_store_explicit(&bank_fade_done,false,memory_order_release);
        bank_state(BANK_FADING_OUT);return;
    }
    if(state==BANK_FADING_OUT) {
        if(!fil_is_open||!metadata_ready(sel_bank_cur)) {
            atomic_store_explicit(&bank_fade_silent,true,memory_order_relaxed);
            atomic_store_explicit(&bank_fade_done,true,memory_order_release);
        }
        if(!bank_transition_audio_hold())return;
        if(!audio_media_acquire()) {metadata_status.last_error=META_TIMEOUT;bank_finish(false);return;}
        bank_job.owned=true;
        bank_job.previous_bank=sel_bank_cur;bank_job.previous_sample=sel_sample_cur;
        bank_job.previous_valid=metadata_ready(sel_bank_cur)&&fil_is_open;
        bank_job.phase[0]=phases[0];bank_job.phase[1]=phases[1];
        bank_job.next_phase=phase_new;bank_job.changed_phase=phase_change;
        bank_job.beat=beat_current;bank_job.exhausted=mute_because_of_playback_type;
        bank_job.silent=atomic_load_explicit(&bank_fade_silent,memory_order_acquire);
        bank_job.variation=sel_variation;bank_job.variant=audio_variant;
        do_open_file_ready=false;fil_current_change_force=false;
        bank_job.stage=0;bank_state(BANK_LOADING);return;
    }
    if(state==BANK_CANCEL) {
        metadata_load_cancel();bank_job.stage=1;bank_state(BANK_LOADING);return;
    }
    if(state==BANK_LOADING||state==BANK_ROLLBACK) {
        unsigned bank=bank_job.rollback?bank_job.previous_bank:bank_job.bank;
        if(bank_job.stage==0) {
            // Rollback may need to close a partial metadata file first.
            if(bank_job.rollback)metadata_load_cancel();
            else if(audio_file_close()!=FR_OK) {
                metadata_status.last_error=META_AUDIO;bank_fail();return;
            }
            bank_job.stage=1;return;
        }
        if(bank_job.stage==1) {
            bank_job.load_started=time_us_32();
            if(!metadata_load_begin(bank,bank_job.load_started)) {bank_fail();return;}
            bank_job.stage=2;return;
        }
        int result=metadata_load_step(time_us_32());
        if(result<0) {bank_fail();return;}
        if(result>0) {bank_job.stage=0;bank_state(BANK_COMMITTING);}
        return;
    }
    if(state==BANK_COMMITTING) {
        unsigned bank=bank_job.rollback?bank_job.previous_bank:bank_job.bank;
        unsigned sample=bank_job.rollback?bank_job.previous_sample:bank_job.sample;
        if((uint32_t)(time_us_32()-bank_job.load_started)>2000000u) {
            metadata_status.last_error=META_TIMEOUT;bank_fail();return;
        }
        char name[32];
        format_sample_filename(name,bank,sample,bank_job.variation+bank_job.variant*2);
        if(audio_file_open(name)!=FR_OK) {metadata_status.last_error=META_AUDIO;bank_fail();return;}
        // All readers are excluded until file, pointers, selection and phase
        // have been published together by releasing filesystem ownership.
        metadata_publish();sel_bank_cur=bank;sel_sample_cur=sample;
        if(bank_job.rollback) {
            phases[0]=bank_job.phase[0];phases[1]=bank_job.phase[1];
            phase_new=bank_job.next_phase;phase_change=bank_job.changed_phase;
            beat_current=bank_job.beat;mute_because_of_playback_type=bank_job.exhausted;
            if(!bank_job.replay && sel_bank_next==bank_job.bank&&sel_sample_next==bank_job.sample) {
                sel_bank_next=bank;sel_sample_next=sample;
            }
        } else {
            SampleInfo *next=banks[bank]->sample[sample].snd[0];
            SampleInfo *old=metadata_selection_valid(bank_job.previous_bank,bank_job.previous_sample)?
                banks[bank_job.previous_bank]->sample[bank_job.previous_sample].snd[0]:NULL;
            int32_t phase=0,beat=0;
            if(!bank_job.silent&&old) {
                phase=round(((double)bank_job.phase[0]*next->size/old->size)*
                    sel_variation_scale[sel_variation]*sel_variation_scale[sel_variation]);
                beat=round((double)bank_job.beat*next->slice_num)/old->slice_num;
            }
            phases[0]=phases[1]=phase_new=phase;phase_change=true;
            beat_current=beat;next->slice_current=(unsigned)beat%next->slice_num;
            mute_because_of_playback_type=false;
            metadata_status.last_error=META_OK;
        }
        bank_finish(true);return;
    }
    if(state==BANK_ERROR) {
        if(bank_job.stage++==0) {metadata_load_cancel();return;}
        audio_file_close();bank_finish(false);
    }
}
#endif
