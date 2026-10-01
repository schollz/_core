static bool audio_file_change_variation(void) {
    if(!metadata_ready(sel_bank_cur))return false;
    format_sample_filename(fil_current_name,sel_bank_cur,sel_sample_cur,sel_variation_next);
    if(audio_file_open(fil_current_name)!=FR_OK)return false;
    phases[0]=round(((float)phases[0]*sel_variation_scale[sel_variation_next])/sel_variation_scale[sel_variation]);
    sel_variation=sel_variation_next;
    realtime_stretch_reset_from_playback_phase();return true;
}
static void audio_media_poll(void) {
    if(fil_current_change&&sel_bank_next!=host_resident)host_requested=sel_bank_next;
}
