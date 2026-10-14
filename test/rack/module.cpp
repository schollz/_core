// Integration test of the actual Rack Module, including Rack's Speex converter.
#include "../../rack/src/Ectocore.cpp"
#include <cassert>
#include <iostream>
static void load(Ectocore &m,const char *folder,float rate){
    m.onSampleRateChange({rate,1.f/rate});m.choose(folder);
    for(unsigned i=0;i<10000&&!m.active;++i){m.process({rate,1.f/rate,int64_t(i)});std::this_thread::sleep_for(std::chrono::milliseconds(1));}
    assert(m.active);m.params[Ectocore::SAMPLE].setValue(1.f);
    CoreState s;core_engine_get_state(m.engine,&s);s.clock_stop=false;core_engine_update_settings(m.engine,&s);
}
static void check_clock_stop(Ectocore &m,float rate){
    m.inputs[Ectocore::CLOCK_IN].setChannels(1);
    for(bool trigger:{false,true})for(bool slice:{false,true}){
        CoreState s;core_engine_get_state(m.engine,&s);
        s.clock_stop=true;s.clock_trigger=trigger;s.clock_slice=slice;
        assert(core_engine_update_settings(m.engine,&s));
        unsigned activeEdges=0,resumedEdges=0,activeAudio=0,resumedAudio=0;
        bool previous=false;
        for(int64_t f=0;f<int64_t(rate*6);++f){
            bool input=(f<int64_t(rate*2)||f>=int64_t(rate*4))&&f%int64_t(rate/4)<int64_t(rate*.01f);
            m.inputs[Ectocore::CLOCK_IN].setVoltage(input?10.f:0.f);
            m.process({rate,1.f/rate,f});
            float clock=m.outputs[Ectocore::CLOCK_OUT].getVoltage();
            float audio=m.outputs[Ectocore::LEFT].getVoltage();
            assert(clock==0.f||clock==10.f);
            if(f>=int64_t(rate)){CoreDisplay d;core_engine_display(m.engine,&d);assert(d.tempo==120);}
            bool high=clock>0;
            if(f>=int64_t(rate)&&f<int64_t(rate*2)){activeEdges+=high&&!previous;activeAudio+=std::abs(audio)>1e-6f;}
            if(f>=int64_t(rate*3)&&f<int64_t(rate*4)){assert(!high);assert(std::abs(audio)<1e-6f);}
            if(f==int64_t(rate*4)){
                CoreDisplay d;core_engine_display(m.engine,&d);
                assert(high&&d.slice==0);
            }
            if(f>=int64_t(rate*5)){resumedEdges+=high&&!previous;resumedAudio+=std::abs(audio)>1e-6f;}
            previous=high;
        }
        assert(activeEdges&&resumedEdges&&activeAudio&&resumedAudio);
    }
    std::cout<<"module: "<<rate<<" Hz, square/trigger and tempo/slice clock stop, 0/10 V and restart at slice zero passed\n";
}
static void check_clock_tempo(Ectocore &m,const char *folder,float rate){
    Ectocore downstream;load(downstream,folder,rate);
    for(auto *module:{&m,&downstream}){
        CoreState s;core_engine_get_state(module->engine,&s);
        s.clock_stop=true;s.clock_slice=false;s.clock_trigger=false;
        assert(core_engine_update_settings(module->engine,&s));
        module->inputs[Ectocore::CLOCK_IN].setChannels(1);
    }
    int64_t frame=0;
    auto step=[&](bool input,unsigned expected){
        m.inputs[Ectocore::CLOCK_IN].setVoltage(input?10.f:0.f);
        m.process({rate,1.f/rate,frame});
        downstream.inputs[Ectocore::CLOCK_IN].setVoltage(m.outputs[Ectocore::CLOCK_OUT].getVoltage());
        downstream.process({rate,1.f/rate,frame++});
        if(expected)for(auto *module:{&m,&downstream}){
            CoreDisplay d;core_engine_display(module->engine,&d);assert(d.tempo==expected);
        }
    };
    auto drive=[&](unsigned bpm,unsigned seconds,bool settled){
        int64_t period=std::llround(rate*30.0/bpm);
        for(int64_t f=0;f<int64_t(rate*seconds);++f)
            step(f%period<int64_t(rate*.01f),settled||f>=int64_t(rate*(seconds-1))?bpm:0);
    };
    drive(120,4,false);
    for(unsigned repeat=0;repeat<2;++repeat){
        for(int64_t f=0;f<int64_t(rate*.5f);++f)step(false,120);
        drive(120,3,true);
    }
    for(int64_t f=0;f<int64_t(rate*2);++f)step(false,120);
    for(auto *module:{&m,&downstream})assert(module->outputs[Ectocore::CLOCK_OUT].getVoltage()==0.f);
    step(true,120);
    for(auto *module:{&m,&downstream}){
        CoreDisplay d;core_engine_display(module->engine,&d);assert(d.slice==0&&d.clock);
    }
    drive(120,2,true);drive(180,10,false);
    std::cout<<"module: "<<rate<<" Hz, chained tempo holds through short/long pauses and follows a real 120-to-180 BPM change\n";
}
static void check_start_tempo(const char *folder) {
    constexpr float rate=44100.f;
    Ectocore m;load(m,folder,rate);
    CoreState s;core_engine_get_state(m.engine,&s);s.start_tempo=130;s.tempo=145;
    assert(core_engine_set_state(m.engine,&s));
    auto *json=m.dataToJson();
    auto *savedState=json_object_get(json,"state");
    CoreState invalidState=m.initial;
    json_object_set_new(savedState,"tempo",json_integer(0));
    assert(!ecto::stateFromJson(savedState,invalidState));
    json_object_set_new(savedState,"tempo",json_integer(145));
    json_object_set_new(savedState,"start_tempo",json_integer(301));
    invalidState=m.initial;assert(!ecto::stateFromJson(savedState,invalidState));
    json_object_set_new(savedState,"start_tempo",json_integer(130));
    Ectocore restored;restored.dataFromJson(json);
    for(unsigned i=0;i<10000&&!restored.active;++i){restored.process({rate,1.f/rate,int64_t(i)});std::this_thread::sleep_for(std::chrono::milliseconds(1));}
    assert(restored.active);core_engine_get_state(restored.engine,&s);assert(s.tempo==130&&s.start_tempo==130);
    // Old patches without the additive field retain the last playing tempo.
    json_object_del(json_object_get(json,"state"),"start_tempo");
    Ectocore legacy;legacy.dataFromJson(json);core_engine_get_state(legacy.engine,&s);
    assert(s.tempo==145&&s.start_tempo==0);json_decref(json);
    // Settings edits, ordinary reload and imports don't change the playing tempo.
    core_engine_get_state(m.engine,&s);s.start_tempo=160;m.edits.push({s});m.process({rate,1.f/rate,0});
    core_engine_get_state(m.engine,&s);assert(s.tempo==145&&s.start_tempo==160);
    auto generation=m.active->library->generation;m.storage.load(folder,m.selectedBank.load());
    for(unsigned i=0;i<10000&&m.active->library->generation==generation;++i){m.process({rate,1.f/rate,int64_t(i)});std::this_thread::sleep_for(std::chrono::milliseconds(1));}
    assert(m.active->library->generation!=generation);core_engine_get_state(m.engine,&s);assert(s.tempo==145&&s.start_tempo==160);
    auto *previous=m.engine;m.resetRequested=true;
    for(unsigned i=0;i<1000&&m.engine==previous;++i){m.process({rate,1.f/rate,int64_t(i)});std::this_thread::sleep_for(std::chrono::milliseconds(1));}
    assert(m.engine!=previous);core_engine_get_state(m.engine,&s);assert(s.tempo==160&&s.start_tempo==160);
    generation=m.active->library->generation;m.storage.load(folder,m.selectedBank.load(),true);
    for(unsigned i=0;i<10000&&m.active->library->generation==generation;++i){m.process({rate,1.f/rate,int64_t(i)});std::this_thread::sleep_for(std::chrono::milliseconds(1));}
    assert(m.active->library->generation!=generation);core_engine_get_state(m.engine,&s);assert(s.tempo==160&&s.start_tempo==0);
    // Folder startup imports the managers' exact text setting, without writing the source.
    auto root=ecto::fs::temp_directory_path()/("rack-startup-module-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    ecto::fs::create_directories(root/"settings");
    struct Cleanup {ecto::fs::path path;~Cleanup(){ecto::fs::remove_all(path);}} cleanup{root};
    ecto::fs::create_directory_symlink(ecto::fs::absolute(folder)/"bank1",root/"bank1");
    {std::ofstream(root/"settings/start_tempo")<<"130\n";}
    Ectocore imported;load(imported,root.string().c_str(),rate);
    core_engine_get_state(imported.engine,&s);assert(s.tempo==130&&s.start_tempo==130);
    m.choose(root.string());generation=m.active->library->generation;
    for(unsigned i=0;i<10000&&m.active->library->generation==generation;++i){m.process({rate,1.f/rate,int64_t(i)});std::this_thread::sleep_for(std::chrono::milliseconds(1));}
    core_engine_get_state(m.engine,&s);assert(s.tempo==130&&s.start_tempo==130);
    std::cout<<"module: fixed startup, patch reopen, legacy patch, folder reload/import and headless reboot passed\n";
}
static void check_startup_playback(const char *folder,float rate) {
    auto root=ecto::fs::temp_directory_path()/("rack-startup-playback-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    ecto::fs::create_directories(root/"settings");
    struct Cleanup {ecto::fs::path path;~Cleanup(){ecto::fs::remove_all(path);}} cleanup{root};
    ecto::fs::copy(ecto::fs::path(folder)/"bank1",root/"bank1",ecto::fs::copy_options::recursive);
    {std::ofstream(root/"settings/startup_playback")<<"wait\n";}
    auto advance=[&](Ectocore &m,bool silent) {
        unsigned sound=0;
        for(int64_t i=0;i<int64_t(rate/2);++i) {
            m.process({rate,1.f/rate,i});
            if(silent)for(int output:{Ectocore::LEFT,Ectocore::RIGHT,Ectocore::CLOCK_OUT,Ectocore::TRIGGER}) {
                if(m.outputs[output].getVoltage()!=0.f) {
                    CoreState state;core_engine_get_state(m.engine,&state);
                    std::cerr<<"startup silence failed: rate="<<rate<<" frame="<<i<<" output="<<output<<" voltage="<<m.outputs[output].getVoltage()<<" stopped="<<state.stopped<<" muted="<<state.muted<<" wait="<<state.startup_wait<<"\n";
                }
                assert(m.outputs[output].getVoltage()==0.f);
            }
            sound+=std::abs(m.outputs[Ectocore::LEFT].getVoltage())>1e-6f;
        }
        if(!silent&&!sound) {
            CoreState state;core_engine_get_state(m.engine,&state);
            std::cerr<<"startup start failed: rate="<<rate<<" stopped="<<state.stopped<<" muted="<<state.muted<<" wait="<<state.startup_wait<<"\n";
        }
        if(!silent)assert(sound);
    };
    Ectocore m;load(m,root.string().c_str(),rate);advance(m,true);
    CoreState s;core_engine_get_state(m.engine,&s);assert(s.startup_wait&&s.stopped&&s.muted);
    // First accepted input clock releases the boot hold through Rack's actual
    // process boundary and sample-rate converter.
    m.inputs[Ectocore::CLOCK_IN].setChannels(1);m.inputs[Ectocore::CLOCK_IN].setVoltage(10.f);
    m.process({rate,1.f/rate,0});core_engine_get_state(m.engine,&s);assert(!s.stopped&&!s.muted);
    m.inputs[Ectocore::CLOCK_IN].setVoltage(0.f);advance(m,false);
    auto *json=m.dataToJson();
    assert(json_is_true(json_object_get(json_object_get(json,"state"),"startup_wait")));
    // Patch reload into an already playing module must discard cached PCM and
    // converter history before producing its next output frame.
    auto previousGeneration=m.active->library->generation;
    m.dataFromJson(json);
    for(unsigned i=0;i<10000&&m.active->library->generation==previousGeneration;++i) {
        m.process({rate,1.f/rate,int64_t(i)});
        assert(m.outputs[Ectocore::LEFT].getVoltage()==0.f&&m.outputs[Ectocore::RIGHT].getVoltage()==0.f);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    assert(m.active->library->generation!=previousGeneration);advance(m,true);
    m.inputs[Ectocore::CLOCK_IN].setVoltage(10.f);m.process({rate,1.f/rate,0});
    m.inputs[Ectocore::CLOCK_IN].setVoltage(0.f);advance(m,false);
    {
        Ectocore restored;restored.onSampleRateChange({rate,1.f/rate});restored.dataFromJson(json);
        core_engine_get_state(restored.engine,&s);assert(s.startup_wait&&s.stopped&&s.muted);
        for(unsigned i=0;i<10000&&!restored.active;++i){restored.process({rate,1.f/rate,int64_t(i)});std::this_thread::sleep_for(std::chrono::milliseconds(1));}
        assert(restored.active);advance(restored,true);
        restored.params[Ectocore::TAP].setValue(1.f);
        advance(restored,false);restored.params[Ectocore::TAP].setValue(0.f);
        core_engine_get_state(restored.engine,&s);assert(!s.stopped&&!s.muted);
    }
    // Saved transport remains authoritative for old patches without the field.
    json_object_del(json_object_get(json,"state"),"startup_wait");
    {
        Ectocore legacy;legacy.onSampleRateChange({rate,1.f/rate});legacy.dataFromJson(json);
        core_engine_get_state(legacy.engine,&s);assert(!s.startup_wait&&!s.stopped);
        for(unsigned i=0;i<10000&&!legacy.active;++i){legacy.process({rate,1.f/rate,int64_t(i)});std::this_thread::sleep_for(std::chrono::milliseconds(1));}
        assert(legacy.active);advance(legacy,false);
    }
    json_object_set_new(json_object_get(json,"state"),"startup_wait",json_string("wait"));
    CoreState invalid=m.initial;assert(!ecto::stateFromJson(json_object_get(json,"state"),invalid));json_decref(json);
    // Reload/import/choosing a different folder only update the preference.
    auto reload=[&](const std::string &path,bool choose) {
        auto generation=m.active->library->generation;
        if(choose)m.choose(path);else m.storage.load(path,m.selectedBank.load(),true);
        for(unsigned i=0;i<10000&&m.active->library->generation==generation;++i){m.process({rate,1.f/rate,int64_t(i)});std::this_thread::sleep_for(std::chrono::milliseconds(1));}
        assert(m.active->library->generation!=generation);
        core_engine_get_state(m.engine,&s);assert(!s.stopped&&!s.muted);
    };
    reload(root.string(),false);advance(m,false);
    reload(folder,true);core_engine_get_state(m.engine,&s);assert(!s.startup_wait);
    reload(root.string(),true);core_engine_get_state(m.engine,&s);assert(s.startup_wait);
    // Preserve a per-module edit over reboot even when the folder differs.
    s.startup_wait=false;m.edits.push({s});m.process({rate,1.f/rate,0});
    auto reboot=[&] {
        auto *previous=m.engine;m.resetRequested=true;
        for(unsigned i=0;i<1000&&m.engine==previous;++i){m.process({rate,1.f/rate,int64_t(i)});std::this_thread::sleep_for(std::chrono::milliseconds(1));}
        assert(m.engine!=previous);core_engine_get_state(m.engine,&s);
    };
    reboot();assert(!s.startup_wait);advance(m,false);
    s.startup_wait=true;m.edits.push({s});m.process({rate,1.f/rate,0});
    core_engine_get_state(m.engine,&s);assert(!s.stopped);reboot();assert(s.startup_wait&&s.stopped&&s.muted);advance(m,true);
    std::cout<<"module: "<<rate<<" Hz startup wait, first clock/button start, patch reopen, legacy patch, imports and reboot passed\n";
}
int main(int argc,char **argv){
    assert(argc==2);rack::Context context;rack::contextSet(&context);context.engine=new rack::engine::Engine;
    check_start_tempo(argv[1]);
    for(float rate:{32000.f,44100.f,48000.f,96000.f,192000.f})check_startup_playback(argv[1],rate);
    for(float rate:{32000.f,44100.f,48000.f,96000.f,192000.f}){
        Ectocore m;assert(m.ezeptocore);load(m,argv[1],rate);double energy=0;unsigned audible=0,longestSilence=0,silence=0;
        for(int64_t i=0;i<int64_t(rate*2);++i){m.process({rate,1.f/rate,i});float v=m.outputs[Ectocore::LEFT].getVoltage();assert(std::isfinite(v)&&std::abs(v)<5.1f);if(i>rate){energy+=v*v;audible+=std::abs(v)>1e-6f;silence=std::abs(v)<1e-6f?silence+1:0;longestSilence=std::max(silence,longestSilence);}}
        assert(energy/rate>.001&&audible>rate*.85&&longestSilence<rate*.03);
        m.ezeptocore=rate==44100.f;auto *json=m.dataToJson();
        {Ectocore restored;restored.dataFromJson(json);assert(restored.ezeptocore.load()==m.ezeptocore.load()&&restored.getFolder()==m.getFolder());CoreState a,b;core_engine_get_state(m.engine,&a);core_engine_get_state(restored.engine,&b);if(a.random_state!=b.random_state||a.tempo!=b.tempo||a.clock_stop!=b.clock_stop){std::cerr<<json_dumps(json,JSON_INDENT(2))<<"\nrestore="<<restored.restorePending<<" rng "<<a.random_state<<" vs "<<b.random_state<<" tempo "<<a.tempo<<" vs "<<b.tempo<<"\n";}assert(a.random_state==b.random_state&&a.tempo==b.tempo&&a.clock_stop==b.clock_stop);}
        json_decref(json);
        check_clock_stop(m,rate);
        check_clock_tempo(m,argv[1],rate);
        // A reboot must also work without a ModuleWidget/UI thread.
        auto *previous=m.engine;m.resetRequested=true;
        for(unsigned i=0;i<1000&&m.engine==previous;++i){m.process({rate,1.f/rate,int64_t(i)});std::this_thread::sleep_for(std::chrono::milliseconds(1));}
        assert(m.engine!=previous);
        std::cout<<"module: "<<rate<<" Hz, audio/patch/headless reboot passed\n";
    }
    rack::contextSet(nullptr);
}
