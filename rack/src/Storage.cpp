// Copyright 2026 Zack Scholl. SPDX-License-Identifier: GPL-3.0-only
#include "Storage.hpp"
#include "../../lib/start_tempo.h"
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
            e->info.sample.slot=s;e->wav=inspect(p);auto &c=e->info.sample;
            require(e->wav.channels==c.channels&&e->wav.rate==44100u*c.rate_multiple&&e->wav.bytes==uint64_t(c.size)+uint64_t(e->wav.rate)*c.channels*2,"WAV and metadata disagree about padding or format");
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
    auto tempo=settings/"start_tempo";if(!fs::exists(tempo))tempo=l->root/"start_tempo";
    if(fs::exists(tempo)){
        uint16_t bpm=0;
        try{auto d=bytes(tempo,16);require(start_tempo_parse(reinterpret_cast<const char*>(d.data()),d.size(),&bpm),"Invalid start tempo");}
        catch(const std::exception&){bpm=0;l->warnings.push_back("Invalid start tempo; using Default (no override).");}
        l->settings["start_tempo"]=bpm?std::to_string(bpm):"default";
    }
    for(unsigned r=0;r<7;++r)l->runePresent[r]=fs::is_directory(settings/"grimoire"/("rune"+std::to_string(r+1)));
    parseSettings(*l,l->preparedSettings);return l;
}
std::unique_ptr<Bank> Storage::prepare(std::shared_ptr<Library> l,unsigned index){
    if(index>=16||!l->banks[index].count){index=0;while(index<16&&!l->banks[index].count)++index;}
    require(index<16,"Empty library");auto result=std::make_unique<Bank>();result->library=std::move(l);result->index=index;
    for(unsigned s=0;s<16;++s)if(auto &e=result->library->entries[index][s]){
        auto &v=result->primary[s];v.resize(size_t(e->wav.bytes));std::ifstream in(path(*result->library,index,s,0),std::ios::binary);in.seekg(std::streamoff(e->wav.offset));
        require(bool(in.read((char*)v.data(),std::streamsize(v.size()))),"Primary WAV changed while loading");result->bytes+=v.size();
    }
    return result;
}
Storage::Storage(){worker=std::thread([this]{run();});}
Storage::~Storage(){stop=true;worker.join();Bank *p;while(ready.pop(p))delete p;while(retired.pop(p))delete p;}
void Storage::load(const std::string &root,unsigned bank,bool importSettings){std::lock_guard<std::mutex> lock(mutex);pendingRoot=root;pendingBank=bank;pendingImport=importSettings;++revision;message="Loading sample folder…";}
std::string Storage::status(){std::lock_guard<std::mutex> lock(mutex);return message;}
bool Storage::read(Bank *b,unsigned bank,unsigned slot,unsigned variant,uint64_t offset,void *out,size_t n){
    if(!b || bank!=b->index || slot>=16 || variant!=0 ||
       !b->library->entries[bank][slot] || offset<44)return false;
    offset-=44;auto &data=b->primary[slot];
    if(offset>data.size() || n>data.size()-offset)return false;
    memcpy(out,data.data()+offset,n);return true;
}
void Storage::run(){
    uint64_t seen=0,generation=0;std::map<uint64_t,std::weak_ptr<Library>> libraries;
    std::shared_ptr<Library> latest;
    auto publish=[&](std::unique_ptr<Bank> bank){
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
        for(auto it=libraries.begin();it!=libraries.end();)if(it->second.expired())it=libraries.erase(it);else ++it;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
static void parseSettings(const Library &l,CoreState &s){
    auto get=[&](const char *key,const char *fallback){auto i=l.settings.find(key);return i==l.settings.end()||i->second.empty()?std::string(fallback):i->second;};
    auto tempo=get("start_tempo","default");start_tempo_parse(tempo.data(),tempo.size(),&s.start_tempo);
    s.clock_stop=get("clock_stop_sync","off")=="on";s.clock_trigger=get("clock_output_trig","off")=="on";s.clock_slice=get("clock_behavior_sync_slice","off")=="on";
    s.bipolar[0]=get("amen_cv","bipolar")=="bipolar";s.bipolar[1]=get("break_cv","bipolar")=="bipolar";s.bipolar[2]=get("sample_cv","bipolar")=="bipolar";
    auto amen=get("amen_behavior","jump");s.amen_behavior=amen=="repeat"?1:amen=="split"?2:0;
    s.sample_mapping=get("sample_cv_mapping","bank")=="1voct";auto reset=get("override_with_reset","none");s.reset_input=reset=="amen"?0:reset=="break"?1:reset=="sample"?2:reset=="clk"?3:-1;
    try{s.brightness=uint8_t(std::clamp(std::stoi(get("brightness","50")),0,100));}catch(...){s.brightness=50;}
    for(unsigned r=0;r<7;++r)for(unsigned fx=0;fx<16;++fx){auto key="grimoire/rune"+std::to_string(r+1)+"/effect"+std::to_string(fx+1);auto it=l.settings.find(key);if(it!=l.settings.end()&&!it->second.empty())s.runes[r][fx]=it->second=="on";}
}
void applySettings(const Library &l,CoreState &s){
    const auto &v=l.preparedSettings;
    s.start_tempo=v.start_tempo;
    s.clock_stop=v.clock_stop;s.clock_trigger=v.clock_trigger;s.clock_slice=v.clock_slice;
    memcpy(s.bipolar,v.bipolar,sizeof s.bipolar);s.amen_behavior=v.amen_behavior;s.sample_mapping=v.sample_mapping;
    s.reset_input=v.reset_input;s.brightness=v.brightness;
    for(unsigned r=0;r<7;++r)if(l.runePresent[r])memcpy(s.runes[r],v.runes[r],sizeof s.runes[r]);
}
}
