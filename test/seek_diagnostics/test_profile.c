#include "audio_profile.h"
#include "seek_diagnostics.h"
#include <assert.h>
#include <stdio.h>
uint32_t zd_test_time;
zd_audio_t zd_audio;
int main(void) {
    zd_audio.metrics[ZD_READ].total_lo=UINT32_MAX-4;
    zd_test_time=100;audio_profile_begin(true,1,2,48,256);
    zd_test_time=120;audio_profile_add(AP_FILTER,110);
    zd_audio.metrics[ZD_READ].total_lo=5;
    audio_profile_starved(125);
    zd_test_time=130;audio_profile_end(3,4);
    assert(audio_profile_records[0].total_us==30);
    assert(audio_profile_records[0].us[AP_FILTER]==10);
    assert(audio_profile_records[0].us[AP_READ]==10);
    assert(audio_profile_records[2].relation==1&&audio_profile_records[2].event==1);
    assert(audio_profile_records[2].source_before==1&&audio_profile_records[2].source_after==3);
    audio_profile_starved(150);
    zd_test_time=200;audio_profile_begin(false,0,0,0,0);
    assert(audio_profile_records[1].relation==2&&audio_profile_records[1].event==2);
    audio_profile_end(0,0);
    assert(audio_profile_records[0].total_us==30);
    zd_test_time=UINT32_MAX-10;audio_profile_begin(true,1,1,48,256);
    zd_test_time=30;audio_profile_end(1,1);
    assert(audio_profile_records[0].total_us==41);
    for(unsigned i=0;i<4;++i)assert(!(audio_profile_records[i].sequence&1));
    puts("profile: inclusive stages, counter/time wrap, active gating and starvation attribution pass");
}
