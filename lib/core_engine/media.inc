// Desktop catalogue adapter. The owner keeps these immutable views alive.
SampleList *banks[16];
static SampleList host_banks[16];
static Sample host_samples[16][16];
static SampleInfo host_info[16][16];
static uint16_t *host_transients[16][16][3];
static unsigned host_generation;
volatile MetadataStatus metadata_status;
bool metadata_ready(unsigned bank) {return bank<16&&host_resident==bank;}
unsigned metadata_generation(void) {return host_generation;}
bool metadata_selection_valid(unsigned b,unsigned s) {return b<16&&banks[b]&&s<banks[b]->num_samples;}
uint8_t metadata_filename_index(unsigned b,unsigned s) {return metadata_selection_valid(b,s)?banks[b]->sample[s].snd[0]->filename_index:0;}
int metadata_ordinal(unsigned b,unsigned slot) {
    for(unsigned s=0;b<16&&s<banks[b]->num_samples;++s)if(metadata_filename_index(b,s)==slot)return (int)s;
    return -1;
}
static FRESULT audio_file_open(const char *path) {
    FRESULT r=f_open(&fil_current,path,FA_READ);fil_is_open=r==FR_OK;return r;
}
static FRESULT audio_file_close(void) {fil_is_open=false;return f_close(&fil_current);}
static bool audio_media_begin(void) {return true;}
static void audio_media_end(bool q) {(void)q;}
