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
static void check_start_tempo(const fs::path &source) {
    auto root=fs::temp_directory_path()/("rack-start-tempo-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root/"settings");
    struct Cleanup {fs::path root;~Cleanup(){fs::remove_all(root);}} cleanup{root};
    // A regular copy also works on Windows, where MinGW may not support symlinks.
    fs::copy(source/"bank1",root/"bank1",fs::copy_options::recursive);
    auto check=[&](unsigned expected,bool warning){auto l=Storage::catalogue(root,43);CoreState state{};state.tempo=145;applySettings(*l,state);assert(state.start_tempo==expected&&state.tempo==145);assert(l->warnings.empty()!=warning);};
    check(0,false);
    {std::ofstream(root/"start_tempo")<<"130\n";}check(130,false);
    for(auto value:{"30","145","300","default"}) {
        {std::ofstream(root/"settings/start_tempo")<<value<<"\r\n";}
        check(std::string(value)=="default"?0:std::stoul(value),false);
    }
    for(auto value:{"","0","29","301","0130","130.5","130x","10000000000000000000"}) {
        {std::ofstream(root/"settings/start_tempo")<<value;}check(0,true);
    }
    fs::remove(root/"settings/start_tempo");check(130,false);
    std::cout<<"storage: startup tempo imports, default, bounds, precedence and invalid-file warnings passed\n";
}
int main(int argc,char **argv){
    assert(argc==2);check_start_tempo(fs::absolute(argv[1]));
    {std::ifstream in(fs::path(argv[1])/"bank1/15.0.wav.info",std::ios::binary);std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)),{});CoreCardInfo info;char error[128];
        assert(core_card_decode(bytes.data(),bytes.size(),&info,error,sizeof error));
        for(size_t n=0;n<bytes.size();++n)assert(!core_card_decode(bytes.data(),n,&info,error,sizeof error));
        bytes[7]|=128;assert(!core_card_decode(bytes.data(),bytes.size(),&info,error,sizeof error));}
    auto l=Storage::catalogue(argv[1],42);
    assert(l->banks[0].count==2&&l->banks[15].count==1&&l->warnings.size()==2);
    assert(l->banks[0].samples[0].slot==0&&l->banks[0].samples[1].slot==15);
    assert(l->banks[0].samples[0].rate_multiple==2&&l->banks[0].samples[0].channels==1);
    assert(l->banks[2].count==5&&l->banks[3].count==4);
    for(unsigned slot=0;slot<4;++slot){assert(l->entries[2][slot]);assert(bool(l->entries[3][slot])==(slot!=1));}
    CoreState settings{};settings.runes[6][15]=true;applySettings(*l,settings);
    assert(settings.brightness==75&&settings.clock_stop&&!settings.bipolar[0]&&settings.sample_mapping==1);
    assert(settings.runes[0][0]&&!settings.runes[0][1]&&settings.runes[6][15]);
    Storage storage;storage.load(argv[1],15);auto *b=awaitBank(storage);
    assert(b->index==15&&b->bytes<10*1024*1024);
    auto &entry=*b->library->entries[15][15];
    std::ifstream file(Storage::path(*b->library,15,15,0),std::ios::binary);
    uint8_t expected[1000],actual[1000];
    // All granular source reads come from the already-resident primary bank.
    for(uint64_t offset:{0ull,65500ull,131071ull,1000000ull,65500ull}){
        file.seekg(entry.wav.offset+offset);assert(file.read((char*)expected,sizeof expected));
        assert(storage.read(b,15,15,0,44+offset,actual,sizeof actual));
        assert(!memcmp(actual,expected,sizeof actual));
    }
    assert(!storage.read(b,15,15,0,44+entry.wav.bytes-1,actual,sizeof actual));
    assert(!storage.read(b,15,15,1,44,actual,sizeof actual));
    for(int bank=0;bank<16;++bank){storage.requestedBank=bank;auto *next=awaitBank(storage);assert(next->index==unsigned(bank));assert(storage.retired.push(b));b=next;}
    storage.load("/missing/ectocore/test/path",0);
    for(unsigned t=0;t<1000&&storage.status().find("missing")==std::string::npos;++t)std::this_thread::sleep_for(std::chrono::milliseconds(1));
    assert(storage.status().find("missing")!=std::string::npos);
    assert(storage.read(b,15,15,0,44,actual,sizeof actual)); // previous bank survives failure
    delete b;
    std::cout<<"storage: 16 banks, sparse slots, legacy core server exports, ignored legacy companions, primary-only tempo matching, 88.2 kHz mono, settings, resident reads, invalid folder passed\n";
}
