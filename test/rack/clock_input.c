// Test the real input detector with precise timestamps and GPIO boundary stubs.
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

static uint32_t now;
static unsigned starts, rises, falls;
static int reported_period;
static uint32_t time_us_32(void) { return now; }
static void gpio_init(unsigned p) { (void)p; }
static void gpio_set_dir(unsigned p,unsigned d) { (void)p;(void)d; }
static void gpio_pull_down(unsigned p) { (void)p; }
static unsigned gpio_get(unsigned p) { (void)p;return 1; }
#define GPIO_IN 0
#include "clock_input.h"

static void start(void) { ++starts; }
static void rise(int period) { ++rises;reported_period=period; }
static void fall(int width) { ++falls;assert(width==0); }
static void pulse(ClockInput *ci,uint32_t interval) {
    unsigned before=starts+rises;
    now=ci->last_time+interval;
    ClockInput_update_raw(ci,1);
    assert(starts+rises==before+1); // Confirmation must never swallow an edge.
    ClockInput_update_raw(ci,0);
    assert(falls==starts+rises);
}
static ClockInput *steady(void) {
    now=0;starts=rises=falls=0;
    ClockInput *ci=ClockInput_create(0,rise,fall,start);assert(ci);
    for(unsigned i=0;i<40;++i)pulse(ci,250000);
    assert(lround(30000000.0/ci->filter->filtered)==120);
    return ci;
}
static void check_gap_and_outliers(void) {
    ClockInput *ci=steady();uint32_t retained=ci->filter->filtered;
    pulse(ci,354671);assert(ci->filter->filtered==retained&&reported_period==(int)retained);
    pulse(ci,250000);assert(ci->filter->filtered==retained);
    pulse(ci,250000);assert(ci->filter->filtered==retained);
    pulse(ci,180000);assert(ci->filter->filtered==retained);
    pulse(ci,250000);assert(ci->filter->filtered==retained);
    pulse(ci,250000);assert(ci->filter->filtered==retained);
    unsigned before=starts;
    pulse(ci,3000000);assert(starts==before+1&&ci->filter->filtered==retained);
    // One interval after a restart cannot change tempo, even if it matches the
    // last candidate from before the stop. The next matching interval can.
    pulse(ci,300000);assert(ci->filter->filtered==retained);
    pulse(ci,300000);assert(ci->filter->filtered>retained);
    retained=ci->filter->filtered;
    pulse(ci,3000000);assert(ci->filter->filtered==retained);
    pulse(ci,300000);assert(ci->filter->filtered==retained);
    pulse(ci,300000);assert(ci->filter->filtered>retained);
    ClockInput_destroy(ci);
}
static void check_tolerance(void) {
    for(unsigned reverse=0;reverse<2;++reverse)for(unsigned outside=0;outside<2;++outside) {
        ClockInput *ci=steady();uint32_t retained=ci->filter->filtered;
        uint32_t a=300000,b=306000+outside;
        pulse(ci,reverse?b:a);assert(ci->filter->filtered==retained);
        pulse(ci,reverse?a:b);
        assert(outside?ci->filter->filtered==retained:ci->filter->filtered>retained);
        ClockInput_destroy(ci);
    }
}
static void check_changes(void) {
    for(unsigned stopped=0;stopped<2;++stopped)for(unsigned slower=0;slower<2;++slower) {
        ClockInput *ci=steady();uint32_t retained=ci->filter->filtered;
        uint32_t period=slower?500000:125000;
        if(stopped)pulse(ci,3000000);
        pulse(ci,period);assert(ci->filter->filtered==retained);
        // Halving a running clock is classified as a restart by the existing
        // raw-edge detector, so two further complete intervals are required.
        if(slower&&!stopped){pulse(ci,period);assert(ci->filter->filtered==retained);}
        pulse(ci,period);assert(ci->filter->filtered!=retained);
        for(unsigned i=0;i<30;++i)pulse(ci,period);
        assert(lround(30000000.0/ci->filter->filtered)==(slower?60:240));
        ClockInput_destroy(ci);
    }
}
static void check_jitter_and_ramp(void) {
    ClockInput *ci=steady();
    for(unsigned i=0;i<100;++i){pulse(ci,i%2?250500:249500);assert(lround(30000000.0/ci->filter->filtered)==120);}
    uint32_t period=250000;
    for(unsigned i=0;i<40;++i){period=period*99/100;pulse(ci,period);}
    assert(ci->filter->filtered<180000); // Gentle continuous changes still track.
    ClockInput_destroy(ci);
    ci=steady();uint32_t retained=ci->filter->filtered;
    for(unsigned i=0;i<40;++i){pulse(ci,i%2?250000:350000);assert(ci->filter->filtered==retained);}
    ClockInput_destroy(ci);
}
int main(void) {
    check_gap_and_outliers();check_tolerance();check_changes();check_jitter_and_ramp();
    puts("clock input: held tempo, every edge delivered, restart reacquisition, 2% boundary, half/double rate, jitter and ramps passed");
}
