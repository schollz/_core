// Exercise the actual buffer-conversion templates, including split/combined
// producers, rather than assuming a rendered buffer is already a DMA buffer.
#include <cassert>
#include <cstdio>
#include <vector>
#define _Static_assert static_assert
extern "C" {
#include "seek_diagnostics.h"
#include "hardware/structs/rosc.h"
uint32_t zd_test_time;
struct zd_test_rosc zd_test_rosc={1};
}
#include "pico/sample_conversion.h"
static void queue(audio_buffer_t *&head,audio_buffer_t *buffer) {
    buffer->next=nullptr;
    if(!head)head=buffer;
    else {audio_buffer_t *tail=head;while(tail->next)tail=tail->next;tail->next=buffer;}
}
static audio_buffer_t *take(audio_buffer_t *&head) {
    audio_buffer_t *buffer=head;if(buffer){head=buffer->next;buffer->next=nullptr;}return buffer;
}
extern "C" audio_buffer_t *get_free_audio_buffer(audio_buffer_pool_t *pool,bool) {return take(pool->free_list);}
extern "C" audio_buffer_t *get_full_audio_buffer(audio_buffer_pool_t *pool,bool) {return take(pool->prepared_list);}
extern "C" void queue_free_audio_buffer(audio_buffer_pool_t *pool,audio_buffer_t *buffer) {queue(pool->free_list,buffer);}
extern "C" void queue_full_audio_buffer(audio_buffer_pool_t *pool,audio_buffer_t *buffer) {queue(pool->prepared_list,buffer);}
struct Buffer {
    std::vector<int16_t> samples;
    mem_buffer_t memory;
    audio_buffer_t audio{};
    Buffer(unsigned frames,audio_buffer_format_t *format):samples(frames*2) {
        memory={reinterpret_cast<uint8_t *>(samples.data()),frames*4};
        audio.buffer=&memory;audio.format=format;audio.max_sample_count=frames;audio.sample_count=frames;
    }
};
static void publish(void) {++zeptocore_diag.request.sequence;zd_service(ZD_IRQ);}
static void conversion(unsigned frames,bool on_give) {
    uint32_t before=zeptocore_diag.irq.data.counters[9];
    zd_test_time=1000;zd_switch_request(0x1234);zd_switch_file(0x9999);assert(!zd_switch_render_tag());
    zd_switch_file(0x1234);uint32_t token=zd_switch_render_tag();assert(token);
    audio_format_t fmt{44100,AUDIO_PCM_FORMAT_S16,AUDIO_CHANNEL_STEREO};
    audio_buffer_format_t buffer_fmt{&fmt,4};
    audio_buffer_pool_t producer{},consumer{};
    std::vector<Buffer> sources,destinations;sources.reserve(12);destinations.reserve(32);
    for(unsigned i=0;i<12;++i) {
        sources.emplace_back(frames,&buffer_fmt);auto &b=sources.back();
        for(unsigned j=0;j<frames*2;++j)b.samples[j]=(i*frames*2+j)%30000;
        b.audio.user_data=i>=5?token:0;queue_full_audio_buffer(&producer,&b.audio);
    }
    for(unsigned i=0;i<32;++i) {
        destinations.emplace_back(256,&buffer_fmt);
        destinations.back().audio.user_data=0xffffffff; // stale previous use
        queue_free_audio_buffer(&consumer,&destinations.back().audio);
    }
    buffer_copying_on_consumer_take_connection cc{};
    cc.core.producer_pool=&producer;cc.core.consumer_pool=&consumer;
    producer_pool_blocking_give_connection pg{};
    pg.core.producer_pool=&producer;pg.core.consumer_pool=&consumer;
    if(on_give)while(auto *b=get_full_audio_buffer(&producer,false))
        producer_pool_blocking_give<Stereo<FmtS16>,Stereo<FmtS16>>(&pg.core,b);
    unsigned position=0,first=5*frames;bool observed=false;
    while(auto *b=on_give?get_full_audio_buffer(&consumer,false):
          consumer_pool_take<Stereo<FmtS16>,Stereo<FmtS16>>(&cc.core,false)) {
        auto *samples=reinterpret_cast<int16_t *>(b->buffer->bytes);
        for(unsigned j=0;j<b->sample_count*2;++j)assert(samples[j]==int16_t((position*2+j)%30000));
        if(position+b->sample_count<=first)assert(b->user_data==0);
        else {
            assert((b->user_data>>9)==(token>>9));
            if(!observed) {
                unsigned offset=first-position;assert((b->user_data&511)==offset);
                zd_switch_dma(b->user_data,11000);
                publish();assert(zeptocore_diag.irq.data.counters[9]==before+1);
                assert(zeptocore_diag.irq.data.counters[10]==10000+
                    uint64_t(offset)*1000000000/zeptocore_diag.header[22]);
                observed=true;
            }
            zd_switch_dma(b->user_data,12000); // duplicates never count twice
        }
        position+=b->sample_count;queue_free_audio_buffer(&consumer,b);
    }
    assert(observed);publish();assert(zeptocore_diag.irq.data.counters[9]==before+1);
}
int main() {
    zd_init(225000000,441);zd_audio_clock(225000000,79,16);
    zeptocore_diag.request.command=ZD_SNAPSHOT;
    for(unsigned frames:{128u,256u,441u})for(bool give:{false,true})conversion(frames,give);
    zd_test_time=0xfffffff0;zd_switch_request(9);zd_switch_file(9);uint32_t token=zd_switch_render_tag();
    zd_switch_request(10); // overlap is explicitly unmeasured
    zd_switch_dma(token,32);publish();assert(zeptocore_diag.irq.data.counters[10]==48);
    assert(zeptocore_diag.irq.data.counters[14]==1);
    zd_test_time=1000;zd_switch_request(11);zd_switch_file(UINT32_MAX);assert(!zd_switch_render_tag());
    zd_test_time=2001001;zd_switch_request(12);publish();assert(zeptocore_diag.irq.data.counters[15]==1);
    puts("audio trace: 128/256/441 producer split/combine, PCM equality, first-frame offsets, wrap, overlaps and expiry pass");
}
