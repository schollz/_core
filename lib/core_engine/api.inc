// Included after the firmware so wrappers can access its state.
static void host_initialize(void) {
    sf=SaveFile_malloc();
    beatrepeat=BeatRepeat_malloc();delay=Delay_malloc();
    Delay_setActive(delay,false);Delay_setDuration(delay,8018);
    combfilter=Comb_malloc();Comb_setActive(combfilter,false,10,10);
    saturation=Saturation_malloc();taptempo=TapTempo_malloc();
    for(unsigned i=0;i<DEBOUNCE_UINT8_NUM;++i)debouncer_uint8[i]=DebounceUint8_malloc();
    resFilter[0]=ResonantFilter_create(0);resFilter[1]=ResonantFilter_create(0);
    envelope_volume=Envelope2_create(BLOCKS_PER_SECOND,0,1,4);
    envelope_pitch=Envelope2_create(BLOCKS_PER_SECOND,0.5,1,1.5);
    envelope_filter=EnvelopeLinearInteger_create(BLOCKS_PER_SECOND,1,resonantfilter_fc_max,0.3);
    noise_wobble=Noise_create(1,BLOCKS_PER_SECOND);
    noise_feedback=Noise_create(2,BLOCKS_PER_SECOND);
    noise_duration=Noise_create(3,BLOCKS_PER_SECOND);
    audio_gate=Gate_create(BLOCKS_PER_SECOND,165);
    freeverb=FV_Reverb_malloc(FV_INITIALROOM,FV_INITIALDAMP,FV_INITIALWET,FV_INITIALDRY);
    freeverb_ready=freeverb!=NULL;
    for(unsigned b=0;b<16;++b) {banks[b]=&host_banks[b];host_banks[b].sample=host_samples[b];}
    for(unsigned p=0;p<32;++p)host_pins[p]=true;
    for(unsigned r=0;r<7;++r)for(unsigned f=0;f<16;++f)grimoire_rune_effect[r][f]=random_integer_in_range(0,1);
    update_repeating_timer_to_bpm(sf->bpm_tempo);
}

// Keep setjmp inside the preprocessed firmware unit: some platforms expose it
// only as a macro. Its stack frame stays alive throughout allocating startup.
static bool host_start_engine(uint64_t seed) {
    if(setjmp(host_create_failure))return false;
    pcg32_srandom_r(&rng,seed,54);
    host_initialize();input_handling();return true;
}

void core_engine_set_catalogue(CoreEngine *e,const CoreBank catalog[16],CoreRead reader,void *user) {
    CoreEngine *old=core_enter(e);host_read=reader;host_read_user=user;
    banks_with_samples_num=0;total_number_samples=0;
    for(unsigned b=0;b<16;++b) {
        host_banks[b].num_samples=catalog[b].count;
        if(catalog[b].count){banks_with_samples[banks_with_samples_num++]=b;total_number_samples+=catalog[b].count;}
        for(unsigned s=0;s<catalog[b].count;++s) {
            const CoreSample *c=&catalog[b].samples[s];SampleInfo *v=&host_info[b][s];
            memset(v,0,sizeof *v);v->size=c->size;v->bpm=c->bpm<30?30:c->bpm>300?300:c->bpm;
            v->filename_index=c->slot;v->num_channels=c->channels-1;v->oversampling=c->rate_multiple-1;
            v->play_mode=c->mode;v->one_shot=c->one_shot;v->tempo_match=c->tempo_match;
            v->splice_trigger=c->splice_ticks;v->splice_variable=c->variable;v->slice_num=c->slice_count;
            v->slice_start=(int32_t*)c->starts;v->slice_stop=(int32_t*)c->stops;v->slice_type=(int8_t*)c->types;
            v->transient_num_1=c->transient_count[0];v->transient_num_2=c->transient_count[1];v->transient_num_3=c->transient_count[2];
            for(unsigned lane=0;lane<3;++lane)host_transients[b][s][lane]=(uint16_t*)c->transients[lane];
            v->transients=host_transients[b][s];host_samples[b][s].snd[0]=v;host_samples[b][s].snd[1]=v;
        }
    }
    if(!metadata_selection_valid(sel_bank_cur,sel_sample_cur)&&banks_with_samples_num) {
        sel_bank_cur=sel_bank_next=banks_with_samples[0];sel_sample_cur=sel_sample_next=0;
    }
    ++host_generation;core_leave(old);
}
void core_engine_set_resident_bank(CoreEngine *e,unsigned b) {
    CoreEngine *old=core_enter(e);
    if(b<16&&banks[b]->num_samples) {
        host_resident=b;host_requested=16;
        sel_bank_cur=sel_bank_next=b;
        if(sel_sample_next>=banks[b]->num_samples)sel_sample_next=0;
        sel_sample_cur=sel_sample_next;
        format_sample_filename(fil_current_name,b,sel_sample_cur,audio_variant * 2);
        audio_file_open(fil_current_name);fil_current_change=false;
        phases[0]=phases[1]=phase_new=0;phase_change=true;first_loop_ever=true;audio_was_muted=true;
        mute_because_of_playback_type=false;realtime_stretch_reset_from_playback_phase();++host_generation;host_selection_changed=true;
    }
    core_leave(old);
}
void core_engine_controls(CoreEngine *e,const CoreControls *v) {
    CoreEngine *old=core_enter(e);host_controls=*v;
    static const uint8_t pins[4]={GPIO_BTN_MODE,GPIO_BTN_MULT,GPIO_BTN_BANK,GPIO_BTN_TAPTEMPO};
    for(unsigned i=0;i<4;++i)host_pins[pins[i]]=!v->buttons[i];
    core_leave(old);
}
void core_engine_clear_holds(CoreEngine *e) {
    CoreEngine *old=core_enter(e);CoreControls v=host_controls;memset(v.buttons,0,sizeof v.buttons);
    core_engine_controls(e,&v);core_leave(old);
}
void core_engine_clock(CoreEngine *e,bool high) {
    CoreEngine *old=core_enter(e);
    if(clockinput&&total_number_samples)ClockInput_update_raw(clockinput,high);
    core_leave(old);
}
static unsigned host_pcm_pos=CORE_BLOCK;
static uint64_t host_next_controls;
void core_engine_process(CoreEngine *e,int16_t out[2]) {
    CoreEngine *old=core_enter(e);
    host_time=(++host_frames*1000000u)/CORE_RATE;
    if(host_time>=host_next_controls){host_next_controls=host_time+1000;input_handling();}
    if(timer.enabled&&host_time>=timer.next){timer.next+=timer.interval;timer_step();}
    if(host_pcm_pos>=CORE_BLOCK){
        host_pcm_pos=0;host_missing=false;
        if(total_number_samples&&metadata_ready(sel_bank_cur)&&fil_is_open)zeptocore_render_audio();
        else memset(host_pcm,0,sizeof host_pcm);
        for(unsigned i=0;i<CORE_BLOCK;++i)for(unsigned c=0;c<2;++c){
            if(host_missing)host_pcm[2*i+c]=i<64?(int32_t)host_last_pcm[c]*(64-i)/64:0;
            else if(host_was_missing&&i<64)host_pcm[2*i+c]=(int32_t)host_pcm[2*i+c]*i/64;
        }
        host_was_missing=host_missing;host_last_pcm[0]=host_pcm[CORE_BLOCK*2-2];host_last_pcm[1]=host_pcm[CORE_BLOCK*2-1];
    }
    out[0]=host_pcm[2*host_pcm_pos];out[1]=host_pcm[2*host_pcm_pos+1];++host_pcm_pos;
    core_leave(old);
}
void core_engine_display(CoreEngine *e,CoreDisplay *v) {
    CoreEngine *old=core_enter(e);memset(v,0,sizeof *v);
    if(ws2812)for(unsigned i=0;i<18;++i) {
        // The desktop LED buffer stores unscaled colors so brightness edits
        // also update a held pattern, without waiting for another LED write.
        uint32_t d=ws2812->data[i],brightness=ws2812->brightness;
        v->rgb[i][0]=((d>>16)&255)*brightness/255;
        v->rgb[i][1]=((d>>24)&255)*brightness/255;
        v->rgb[i][2]=((d>>8)&255)*brightness/255;
    }
    for(unsigned i=0;i<4;++i)v->mode[i]=!host_pins[gpio_mode_leds[i]];
    v->tap=!host_pins[GPIO_LED_TAPTEMPO];v->clock=host_pins[GPIO_CLOCK_OUT];v->trigger=host_pins[GPIO_TRIG_OUT];
    v->stopped=playback_stopped;v->muted=button_mute;v->bank=sel_bank_cur;
    v->slot=metadata_filename_index(sel_bank_cur,sel_sample_cur);v->tempo=sf->bpm_tempo;v->rune=grimoire_rune;
    if(metadata_selection_valid(sel_bank_cur,sel_sample_cur))v->slice=banks[sel_bank_cur]->sample[sel_sample_cur].snd[0]->slice_current;
    for(unsigned i=0;i<16;++i)if(sf->fx_active[i])v->effects|=1u<<i;
    v->requested_bank=host_requested;v->reboot_requested=host_reboot;v->underruns=host_underruns;
    core_leave(old);
}
void core_engine_get_state(CoreEngine *e,CoreState *v) {
    CoreEngine *old=core_enter(e);memset(v,0,sizeof *v);v->version=1;
    v->tempo=sf->bpm_tempo;v->volume=sf->vol;v->pitch=sf->pitch_val_index;
    v->start_tempo=global_start_tempo;
    v->bank=sel_bank_cur;v->slot=metadata_filename_index(sel_bank_cur,sel_sample_cur);
    v->rune=grimoire_rune;v->division=ectocore_clock_selected_division;v->trigger_mode=ectocore_trigger_mode;
    v->stopped=playback_stopped;v->muted=button_mute;v->stay_in_sync=sf->stay_in_sync;
    memcpy(v->effects,sf->fx_active,sizeof v->effects);memcpy(v->effect_params,sf->fx_param,sizeof v->effect_params);
    v->brightness=global_brightness;v->amen_behavior=global_amen_cv_behavior;v->sample_mapping=global_sample_cv_mapping;
    v->bipolar[0]=global_amen_cv_bipolar;v->bipolar[1]=global_break_cv_bipolar;v->bipolar[2]=global_sample_cv_bipolar;
    v->clock_stop=clock_start_stop_sync;v->clock_trigger=clock_output_trig;v->clock_slice=clock_behavior_sync_slice;v->reset_input=cv_reset_override;
    memcpy(v->runes,grimoire_rune_effect,sizeof v->runes);
    v->amiga=mode_amiga_index;v->saturation=mode_digital_saturation;v->smear=mode_digital_smear;
    v->jitter=mode_digital_jitter;v->chaos=mode_chaos_trembler;v->jump_probability=probability_of_random_jump;
    v->retrigger_probability=planned_retrig_probability;v->filter_index=global_filter_index;v->filter_type=global_filter_lphp;
    v->random_state=rng.state;v->random_increment=rng.inc;
    memcpy(v->sequence,random_sequence_arr,sizeof v->sequence);v->sequence_length=random_sequence_length;
    core_leave(old);
}
bool core_engine_set_state(CoreEngine *e,const CoreState *v) {
    if(!start_tempo_valid(v->start_tempo))return false;
    if(v->version!=1||v->tempo<30||v->tempo>300||v->bank>15||v->slot>15||v->rune>6||v->pitch>=PITCH_VAL_MAX||
       v->division>=ECTOCORE_CLOCK_NUM_DIVISIONS||v->trigger_mode>3||v->amen_behavior>2||v->sample_mapping>1||
       v->reset_input< -1||v->reset_input>3||v->sequence_length>64||v->filter_index>resonantfilter_fc_max||v->amiga>37||v->saturation>100||v->smear>100||v->jitter>100||v->chaos>100||v->jump_probability>100||v->retrigger_probability>100)return false;
    CoreEngine *old=core_enter(e);sf->bpm_tempo=v->tempo;sf->vol=v->volume>VOLUME_STEPS?VOLUME_STEPS:v->volume;sf->pitch_val_index=v->pitch;
    global_start_tempo=v->start_tempo;
    sel_bank_next=v->bank;int ordinal=metadata_ordinal(v->bank,v->slot);sel_sample_next=ordinal<0?0:ordinal;
    if(total_number_samples){fil_current_change=true;if(host_resident!=v->bank)host_requested=v->bank;}
    grimoire_rune=v->rune;ectocore_clock_selected_division=v->division;ectocore_trigger_mode=v->trigger_mode;
    playback_stopped=v->stopped;button_mute=v->muted;do_restart_playback=!v->stopped;do_stop_playback=false;sf->stay_in_sync=v->stay_in_sync;
    memcpy(sf->fx_active,v->effects,sizeof v->effects);memcpy(sf->fx_param,v->effect_params,sizeof v->effect_params);
    global_brightness=v->brightness>100?100:v->brightness;global_amen_cv_behavior=v->amen_behavior;global_sample_cv_mapping=v->sample_mapping;
    global_amen_cv_bipolar=v->bipolar[0];global_break_cv_bipolar=v->bipolar[1];global_sample_cv_bipolar=v->bipolar[2];
    clock_start_stop_sync=v->clock_stop;clock_output_trig=v->clock_trigger;clock_behavior_sync_slice=v->clock_slice;cv_reset_override=v->reset_input;
    memcpy(grimoire_rune_effect,v->runes,sizeof v->runes);
    mode_amiga_index=v->amiga>37?37:v->amiga;mode_digital_saturation=v->saturation;mode_digital_smear=v->smear;
    mode_digital_jitter=v->jitter;mode_chaos_trembler=v->chaos;probability_of_random_jump=v->jump_probability;
    planned_retrig_probability=v->retrigger_probability;global_filter_index=v->filter_index;global_filter_lphp=v->filter_type?1:0;
    rng.state=v->random_state;rng.inc=v->random_increment|1;
    memcpy(random_sequence_arr,v->sequence,sizeof v->sequence);random_sequence_length=v->sequence_length;
    for(unsigned i=0;i<16;++i)update_fx(i);
    // Effect initialization may draw random values; patch recall preserves the
    // saved generator position for the next musical event.
    rng.state=v->random_state;rng.inc=v->random_increment|1;
    for(unsigned i=0;i<2;++i){ResonantFilter_setFilterType(resFilter[i],global_filter_lphp);ResonantFilter_setFc(resFilter[i],global_filter_index);}
    WS2812_set_brightness(ws2812,global_brightness);update_gpios_for_mode();update_repeating_timer_to_bpm(sf->bpm_tempo);
    core_leave(old);return true;
}
bool core_engine_update_settings(CoreEngine *e,const CoreState *v) {
    if(!start_tempo_valid(v->start_tempo))return false;
    if(v->amen_behavior>2||v->sample_mapping>1||v->reset_input< -1||v->reset_input>3)return false;
    CoreEngine *old=core_enter(e);
    global_start_tempo=v->start_tempo;
    global_brightness=v->brightness>100?100:v->brightness;
    global_amen_cv_behavior=v->amen_behavior;global_sample_cv_mapping=v->sample_mapping;
    global_amen_cv_bipolar=v->bipolar[0];global_break_cv_bipolar=v->bipolar[1];global_sample_cv_bipolar=v->bipolar[2];
    clock_start_stop_sync=v->clock_stop;clock_output_trig=v->clock_trigger;clock_behavior_sync_slice=v->clock_slice;cv_reset_override=v->reset_input;
    memcpy(grimoire_rune_effect,v->runes,sizeof v->runes);
    WS2812_set_brightness(ws2812,global_brightness);core_leave(old);return true;
}
void core_engine_apply_start_tempo(CoreEngine *e) {
    CoreEngine *old=core_enter(e);
    sf->bpm_tempo=start_tempo_resolve(global_start_tempo,sf->bpm_tempo);
    update_repeating_timer_to_bpm(sf->bpm_tempo);
    core_leave(old);
}
