static void audio_media_poll(void) {
    if(fil_current_change&&sel_bank_next!=host_resident)host_requested=sel_bank_next;
}
