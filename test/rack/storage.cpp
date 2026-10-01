#include "../../rack/src/Storage.hpp"
#include <cassert>
#include <chrono>
#include <cstring>
#include <iostream>
using namespace ecto;
static Bank *awaitBank(Storage &s) {
    Bank *b=nullptr;
    for(unsigned i=0;i<10000&&!s.ready.pop(b);++i)std::this_thread::sleep_for(std::chrono::milliseconds(1));
    assert(b);s.activeGeneration=b->library->generation;return b;
}
int main(int argc,char **argv){
    assert(argc==2);
    {std::ifstream in(fs::path(argv[1])/"bank1/15.0.wav.info",std::ios::binary);std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)),{});CoreCardInfo info;char error[128];
        assert(core_card_decode(bytes.data(),bytes.size(),&info,error,sizeof error));
        for(size_t n=0;n<bytes.size();++n)assert(!core_card_decode(bytes.data(),n,&info,error,sizeof error));
        bytes[7]|=128;assert(!core_card_decode(bytes.data(),bytes.size(),&info,error,sizeof error));}
    auto l=Storage::catalogue(argv[1],42);
    assert(l->banks[0].count==2&&l->banks[15].count==1&&l->warnings.size()==1);
    assert(l->banks[0].samples[0].slot==0&&l->banks[0].samples[1].slot==15);
    assert(l->banks[0].samples[0].rate_multiple==2&&l->banks[0].samples[0].channels==1);
    CoreState settings{};settings.runes[6][15]=true;applySettings(*l,settings);
    assert(settings.brightness==75&&settings.clock_stop&&!settings.bipolar[0]&&settings.sample_mapping==1);
    assert(settings.runes[0][0]&&!settings.runes[0][1]&&settings.runes[6][15]);
    Storage storage;storage.load(argv[1],15);auto *b=awaitBank(storage);
    assert(b->index==15&&b->bytes<10*1024*1024);
    auto &entry=*b->library->entries[15][15];assert(entry.wav[1].bytes>Storage::pageBytes*Storage::pageCount);
    std::ifstream file(Storage::path(*b->library,15,15,1),std::ios::binary);
    uint8_t expected[1000],actual[1000];
    // Cache misses followed by hits, including cross-page and >64 MiB seeks.
    for(uint64_t offset:{0ull,65500ull,131071ull,69000000ull,1000000ull,65500ull}){
        file.seekg(entry.wav[1].offset+offset);assert(file.read((char*)expected,sizeof expected));
        bool hit=false;for(unsigned t=0;t<10000&&!hit;++t){hit=storage.read(b,15,15,1,44+offset,actual,sizeof actual);if(!hit)std::this_thread::sleep_for(std::chrono::milliseconds(1));}
        assert(hit&&!memcmp(actual,expected,sizeof actual));
    }
    assert(!storage.read(b,15,15,1,44+entry.wav[1].bytes-1,actual,sizeof actual));
    for(int bank=0;bank<16;++bank){storage.requestedBank=bank;auto *next=awaitBank(storage);assert(next->index==unsigned(bank));assert(storage.retired.push(b));b=next;}
    storage.load("/missing/ectocore/test/path",0);
    for(unsigned t=0;t<1000&&storage.status().find("missing")==std::string::npos;++t)std::this_thread::sleep_for(std::chrono::milliseconds(1));
    assert(storage.status().find("missing")!=std::string::npos);
    assert(storage.read(b,15,15,0,44,actual,sizeof actual)); // previous bank survives failure
    delete b;
    std::cout<<"storage: 16 banks, sparse slots, 88.2 kHz mono, settings, bounded streaming, invalid folder passed\n";
}
