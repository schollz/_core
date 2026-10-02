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
int main(int argc,char **argv){
    assert(argc==2);rack::Context context;rack::contextSet(&context);context.engine=new rack::engine::Engine;
    for(float rate:{32000.f,44100.f,48000.f,96000.f,192000.f}){
        Ectocore m;assert(m.ezeptocore);load(m,argv[1],rate);double energy=0;unsigned audible=0,longestSilence=0,silence=0;
        for(int64_t i=0;i<int64_t(rate*2);++i){m.process({rate,1.f/rate,i});float v=m.outputs[Ectocore::LEFT].getVoltage();assert(std::isfinite(v)&&std::abs(v)<5.1f);if(i>rate){energy+=v*v;audible+=std::abs(v)>1e-6f;silence=std::abs(v)<1e-6f?silence+1:0;longestSilence=std::max(silence,longestSilence);}}
        assert(energy/rate>.001&&audible>rate*.85&&longestSilence<rate*.03);
        m.ezeptocore=rate==44100.f;auto *json=m.dataToJson();
        {Ectocore restored;restored.dataFromJson(json);assert(restored.ezeptocore.load()==m.ezeptocore.load()&&restored.getFolder()==m.getFolder());CoreState a,b;core_engine_get_state(m.engine,&a);core_engine_get_state(restored.engine,&b);if(a.random_state!=b.random_state||a.tempo!=b.tempo||a.clock_stop!=b.clock_stop){std::cerr<<json_dumps(json,JSON_INDENT(2))<<"\nrestore="<<restored.restorePending<<" rng "<<a.random_state<<" vs "<<b.random_state<<" tempo "<<a.tempo<<" vs "<<b.tempo<<"\n";}assert(a.random_state==b.random_state&&a.tempo==b.tempo&&a.clock_stop==b.clock_stop);}
        json_decref(json);
        check_clock_stop(m,rate);
        // A reboot must also work without a ModuleWidget/UI thread.
        auto *previous=m.engine;m.resetRequested=true;
        for(unsigned i=0;i<1000&&m.engine==previous;++i){m.process({rate,1.f/rate,int64_t(i)});std::this_thread::sleep_for(std::chrono::milliseconds(1));}
        assert(m.engine!=previous);
        std::cout<<"module: "<<rate<<" Hz, audio/patch/headless reboot passed\n";
    }
    rack::contextSet(nullptr);
}
