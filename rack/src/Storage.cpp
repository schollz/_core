// Copyright 2026 Zack Scholl. SPDX-License-Identifier: GPL-3.0-only
#include "Storage.hpp"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace ecto {
static void parseSettings(const Library&,CoreState&);
static void require(bool b,const std::string &s){if(!b)throw std::runtime_error(s);}
static uint16_t u16(const uint8_t *p){return p[0]|uint16_t(p[1])<<8;}
static uint32_t u32(const uint8_t *p){return u16(p)|uint32_t(u16(p+2))<<16;}
static std::vector<uint8_t> bytes(const fs::path &p,size_t limit){
    auto n=fs::file_size(p);require(n<=limit,"File exceeds supported size: "+p.string());
    std::vector<uint8_t> result(size_t(n),0);std::ifstream in(p,std::ios::binary);
    require(bool(in.read(reinterpret_cast<char*>(result.data()),std::streamsize(n))),"Cannot read "+p.string());return result;
}
fs::path Storage::path(const Library &l,unsigned b,unsigned s,unsigned v){return l.root/("bank"+std::to_string(b+1))/(std::to_string(s)+"."+std::to_string(v)+".wav");}
Wav Storage::inspect(const fs::path &p){
    std::ifstream in(p,std::ios::binary);uint8_t h[16]{};auto size=fs::file_size(p);
    require(bool(in.read((char*)h,12))&&!memcmp(h,"RIFF",4)&&!memcmp(h+8,"WAVE",4),"Expected RIFF WAV");
    require(uint64_t(u32(h+4))+8==size,"Truncated or extended WAV");Wav w;bool fmt=false,data=false;
    while(uint64_t(in.tellg())+8<=size){
        require(bool(in.read((char*)h,8)),"Truncated chunk");uint32_t n=u32(h+4);auto at=uint64_t(in.tellg());
        require(n<=size-at,"WAV chunk exceeds file");
        if(!memcmp(h,"fmt ",4)){
            require(!fmt&&n>=16&&bool(in.read((char*)h,16)),"Invalid WAV format");fmt=true;
            w.channels=u16(h+2);w.rate=u32(h+4);
            require(u16(h)==1&&u16(h+14)==16&&(w.channels==1||w.channels==2)&&(w.rate==44100||w.rate==88200)&&u16(h+12)==w.channels*2&&u32(h+8)==w.rate*w.channels*2,"Unsupported card PCM");
        }else if(!memcmp(h,"data",4)){require(!data,"Duplicate WAV data");data=true;w.offset=at;w.bytes=n;}
        in.seekg(std::streamoff(at+n+(n&1)));require(bool(in),"Cannot seek WAV");
    }
    require(fmt&&data&&w.bytes%(w.channels*2)==0,"Missing or unaligned PCM");return w;
}
std::shared_ptr<Library> Storage::catalogue(const fs::path &root,uint64_t generation){
    require(fs::is_directory(root),"Sample folder is missing");auto l=std::make_shared<Library>();l->root=fs::absolute(root);l->generation=generation;
    unsigned count=0;
    for(unsigned b=0;b<16;++b)for(unsigned s=0;s<16;++s){
        auto p=path(*l,b,s,0),info=p;info+=".info";
        if(!fs::exists(p)&&!fs::exists(info))continue;
        try{
            auto e=std::make_unique<Entry>();auto data=bytes(info,8192);char error[128];
            require(core_card_decode(data.data(),data.size(),&e->info,error,sizeof error),error);
            e->info.sample.slot=s;e->wav[0]=inspect(p);auto &c=e->info.sample;
            require(e->wav[0].channels==c.channels&&e->wav[0].rate==44100u*c.rate_multiple&&e->wav[0].bytes==uint64_t(c.size)+uint64_t(e->wav[0].rate)*c.channels*2,"WAV and metadata disagree about padding or format");
            auto companion=path(*l,b,s,1);e->companion=fs::exists(companion);
            if(e->companion){
                e->wav[1]=inspect(companion);auto ci=companion;ci+=".info";auto cd=bytes(ci,8192);CoreCardInfo temp;
                require(core_card_decode(cd.data(),cd.size(),&temp,error,sizeof error),error);
                require(e->wav[1].channels==c.channels&&e->wav[1].rate==e->wav[0].rate&&uint64_t(temp.sample.size)==uint64_t(c.size)*8&&e->wav[1].bytes==uint64_t(temp.sample.size)+uint64_t(e->wav[1].rate)*c.channels*2,"Invalid eight-times companion");
            }
            require(e->companion||(c.one_shot&&!c.tempo_match),"Required .1.wav companion missing; wait for sample manager to finish");
            l->banks[b].samples[l->banks[b].count++]=e->info.sample;l->entries[b][s]=std::move(e);++count;
        }catch(const std::exception &e){l->warnings.push_back("Bank "+std::to_string(b+1)+", sample "+std::to_string(s+1)+": "+e.what());}
    }
    require(count>0,l->warnings.empty()?"No prepared samples found":l->warnings.front());
    auto settings=l->root/"settings";
    if(fs::is_directory(settings))for(auto &f:fs::recursive_directory_iterator(settings))if(f.is_regular_file()){
        auto key=fs::relative(f.path(),settings).generic_string();auto dash=key.rfind('-');
        if(dash!=std::string::npos){auto base=key.substr(0,dash),value=key.substr(dash+1);auto it=l->settings.find(base);
            if(it!=l->settings.end()&&it->second!=value){l->warnings.push_back("Conflicting setting: "+base);it->second="";}else l->settings[base]=value;}
    }
    auto mapping=settings/"sample_cv_mapping";if(!fs::exists(mapping))mapping=l->root/"sample_cv_mapping";
    if(fs::exists(mapping)){auto d=bytes(mapping,16);std::string s(d.begin(),d.end());while(!s.empty()&&isspace(static_cast<unsigned char>(s.back())))s.pop_back();l->settings["sample_cv_mapping"]=s=="1voct"?"1voct":"bank";}
    for(unsigned r=0;r<7;++r)l->runePresent[r]=fs::is_directory(settings/"grimoire"/("rune"+std::to_string(r+1)));
    parseSettings(*l,l->preparedSettings);return l;
}
std::unique_ptr<Bank> Storage::prepare(std::shared_ptr<Library> l,unsigned index){
    if(index>=16||!l->banks[index].count){index=0;while(index<16&&!l->banks[index].count)++index;}
    require(index<16,"Empty library");auto result=std::make_unique<Bank>();result->library=std::move(l);result->index=index;
    for(unsigned s=0;s<16;++s)if(auto &e=result->library->entries[index][s]){
        auto &v=result->primary[s];v.resize(size_t(e->wav[0].bytes));std::ifstream in(path(*result->library,index,s,0),std::ios::binary);in.seekg(std::streamoff(e->wav[0].offset));
        require(bool(in.read((char*)v.data(),std::streamsize(v.size()))),"Primary WAV changed while loading");result->bytes+=v.size();
    }
    return result;
}
Storage::Storage():pages(new Page[pageCount]){worker=std::thread([this]{run();});}
Storage::~Storage(){stop=true;worker.join();Bank *p;while(ready.pop(p))delete p;while(retired.pop(p))delete p;}
void Storage::load(const std::string &root,unsigned bank,bool importSettings){std::lock_guard<std::mutex> lock(mutex);pendingRoot=root;pendingBank=bank;pendingImport=importSettings;++revision;message="Loading sample folder…";}
std::string Storage::status(){std::lock_guard<std::mutex> lock(mutex);return message;}
size_t Storage::hash(const Request &r){return size_t((r.generation*2654435761u)^(r.bank*65537u)^(r.slot*257u)^r.page)%pageCount;}
static bool equal(const Request&a,const Request&b){return a.generation==b.generation&&a.bank==b.bank&&a.slot==b.slot&&a.page==b.page;}
bool Storage::cached(const Request &r,size_t offset,void *dst,size_t n){
    auto &p=pages[hash(r)];unsigned readyState=1;
    if(p.state.compare_exchange_strong(readyState,2,std::memory_order_acquire)){
        bool ok=equal(p.key,r)&&offset+n<=p.size;
        if(ok)memcpy(dst,p.data.data()+offset,n);
        p.state.store(1,std::memory_order_release);if(ok)return true;
    }
    requests.push(r);return false;
}
bool Storage::read(Bank *b,unsigned bank,unsigned slot,unsigned variant,uint64_t offset,void *out,size_t n){
    if(!b||bank!=b->index||slot>=16||!b->library->entries[bank][slot]||offset<44)return false;
    offset-=44;auto &e=*b->library->entries[bank][slot];
    if(variant==0){auto &data=b->primary[slot];if(offset>data.size()||n>data.size()-offset)return false;memcpy(out,data.data()+offset,n);return true;}
    if(variant!=1||!e.companion||offset>e.wav[1].bytes||n>e.wav[1].bytes-offset)return false;
    bool ok=true;auto *dst=static_cast<uint8_t*>(out);Request r{b->library->generation,0,uint8_t(bank),uint8_t(slot)};
    while(n){r.page=uint32_t(offset/pageBytes);size_t at=offset%pageBytes,take=std::min(n,pageBytes-at);
        if(!cached(r,at,dst,take)){memset(dst,0,take);ok=false;}offset+=take;dst+=take;n-=take;}
    auto last=r.page;if((uint64_t(last)+1)*pageBytes<e.wav[1].bytes){r.page=last+1;requests.push(r);}if(last){r.page=last-1;requests.push(r);}
    if(!ok)misses.fetch_add(1,std::memory_order_relaxed);return ok;
}
void Storage::fill(const Request &r,const Library &l){
    if(r.bank>=16||r.slot>=16)return;auto &entry=l.entries[r.bank][r.slot];if(!entry||!entry->companion)return;
    auto &p=pages[hash(r)];unsigned state=p.state.load(std::memory_order_acquire);if(state==1&&equal(p.key,r))return;
    if(state==2||state==3||!p.state.compare_exchange_strong(state,3,std::memory_order_acquire))return;
    try{
        uint64_t offset=uint64_t(r.page)*pageBytes;require(offset<entry->wav[1].bytes,"Page outside companion");
        size_t n=size_t(std::min<uint64_t>(pageBytes,entry->wav[1].bytes-offset));
        std::ifstream in(path(l,r.bank,r.slot,1),std::ios::binary);in.seekg(std::streamoff(entry->wav[1].offset+offset));
        require(bool(in.read((char*)p.data.data(),std::streamsize(n))),"Companion changed or disappeared");p.size=n;p.key=r;p.state.store(1,std::memory_order_release);
    }catch(...){p.state.store(0,std::memory_order_release);}
}
void Storage::run(){
    uint64_t seen=0,generation=0;std::map<uint64_t,std::weak_ptr<Library>> libraries;
    std::shared_ptr<Library> latest;
    auto publish=[&](std::unique_ptr<Bank> bank){
        // Warm first/last pages and slice starts before publication.
        auto &l=*bank->library;for(unsigned s=0;s<16;++s)if(auto &e=l.entries[bank->index][s])if(e->companion){
            fill({l.generation,0,uint8_t(bank->index),uint8_t(s)},l);
            fill({l.generation,uint32_t((e->wav[1].bytes-1)/pageBytes),uint8_t(bank->index),uint8_t(s)},l);
            for(unsigned i=0;i<e->info.sample.slice_count;++i){
                auto offset=uint64_t(e->info.starts[i])*8+uint64_t(e->wav[1].rate)*e->wav[1].channels;
                fill({l.generation,uint32_t(offset/pageBytes),uint8_t(bank->index),uint8_t(s)},l);
            }
        }
        Bank *raw=bank.release();while(!stop&&!ready.push(raw))std::this_thread::sleep_for(std::chrono::milliseconds(1));if(stop)delete raw;
    };
    while(!stop){
        Bank *dead;while(retired.pop(dead))delete dead;
        std::string root;unsigned bank=0;bool import=false;uint64_t rev;
        {std::lock_guard<std::mutex> lock(mutex);rev=revision;if(rev!=seen){root=pendingRoot;bank=pendingBank;import=pendingImport;}}
        try{
            if(rev!=seen){seen=rev;auto l=catalogue(fs::u8path(root),++generation);auto prepared=prepare(l,bank);l->importSettings=import;
                {std::lock_guard<std::mutex> lock(mutex);if(revision!=rev)continue;}
                libraries[l->generation]=l;latest=l;publish(std::move(prepared));
                std::lock_guard<std::mutex> lock(mutex);message=l->warnings.empty()?"Ready":("Ready — "+l->warnings.front());}
            int requested=requestedBank.exchange(-1);auto it=libraries.find(activeGeneration.load());
            if(requested>=0&&it!=libraries.end())if(auto l=it->second.lock())publish(prepare(l,unsigned(requested)));
        }catch(const std::exception &e){std::lock_guard<std::mutex> lock(mutex);message=e.what();}
        Request request;unsigned work=0;
        while(work++<128&&requests.pop(request)){auto it=libraries.find(request.generation);if(it!=libraries.end())if(auto l=it->second.lock())fill(request,*l);}
        for(auto it=libraries.begin();it!=libraries.end();)if(it->second.expired())it=libraries.erase(it);else ++it;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
static void parseSettings(const Library &l,CoreState &s){
    auto get=[&](const char *key,const char *fallback){auto i=l.settings.find(key);return i==l.settings.end()||i->second.empty()?std::string(fallback):i->second;};
    s.clock_stop=get("clock_stop_sync","off")=="on";s.clock_trigger=get("clock_output_trig","off")=="on";s.clock_slice=get("clock_behavior_sync_slice","off")=="on";
    s.bipolar[0]=get("amen_cv","bipolar")=="bipolar";s.bipolar[1]=get("break_cv","bipolar")=="bipolar";s.bipolar[2]=get("sample_cv","bipolar")=="bipolar";
    auto amen=get("amen_behavior","jump");s.amen_behavior=amen=="repeat"?1:amen=="split"?2:0;
    s.sample_mapping=get("sample_cv_mapping","bank")=="1voct";auto reset=get("override_with_reset","none");s.reset_input=reset=="amen"?0:reset=="break"?1:reset=="sample"?2:reset=="clk"?3:-1;
    try{s.brightness=uint8_t(std::clamp(std::stoi(get("brightness","50")),0,100));}catch(...){s.brightness=50;}
    for(unsigned r=0;r<7;++r)for(unsigned fx=0;fx<16;++fx){auto key="grimoire/rune"+std::to_string(r+1)+"/effect"+std::to_string(fx+1);auto it=l.settings.find(key);if(it!=l.settings.end()&&!it->second.empty())s.runes[r][fx]=it->second=="on";}
}
void applySettings(const Library &l,CoreState &s){
    const auto &v=l.preparedSettings;
    s.clock_stop=v.clock_stop;s.clock_trigger=v.clock_trigger;s.clock_slice=v.clock_slice;
    memcpy(s.bipolar,v.bipolar,sizeof s.bipolar);s.amen_behavior=v.amen_behavior;s.sample_mapping=v.sample_mapping;
    s.reset_input=v.reset_input;s.brightness=v.brightness;
    for(unsigned r=0;r<7;++r)if(l.runePresent[r])memcpy(s.runes[r],v.runes[r],sizeof s.runes[r]);
}
}
