// Copyright 2026 Zack Scholl. SPDX-License-Identifier: GPL-3.0-only
#include <rack.hpp>
#include <osdialog.h>
#include "Storage.hpp"
#include "State.hpp"
#include <atomic>
#include <cstring>

using namespace rack;
Plugin *pluginInstance;

struct Ectocore : engine::Module {
    enum Params {BREAK,GRIMOIRE,AMEN,JUMP,SAMPLE,MODE,MULT,BANK,TAP,NUM_PARAMS};
    enum Inputs {AMEN_CV,BREAK_CV,SAMPLE_CV,CLOCK_IN,NUM_INPUTS};
    enum Outputs {LEFT,RIGHT,TRIGGER,CLOCK_OUT,NUM_OUTPUTS};
    enum Lights {RING,MODIFIERS=48,MODES=54,TAP_LIGHT=58,TRIGGER_LIGHT,CLOCK_LIGHT,NUM_LIGHTS};
    struct Update {CoreState state;};
    CoreEngine *engine=nullptr;
    ecto::Storage storage;
    ecto::Bank *active=nullptr;
    ecto::Queue<Update,16> edits;
    ecto::Queue<CoreState,8> snapshots;
    CoreState initial{},uiState{},restoreState{};
    bool restorePending=false;
    std::atomic<bool> ezeptocore{true};
    std::atomic<unsigned> latch{0};
    std::atomic<bool> resetRequested{false};
    std::atomic<unsigned> selectedBank{0},selectedSlot{0};
    std::atomic<bool> running{false};
    std::mutex pathMutex;
    std::string folder;
    dsp::SchmittTrigger clockTrigger;
    dsp::SampleRateConverter<2> resampler;
    dsp::Frame<2> resampleInput{},resampleOutput[64]{},outputQueue[256]{};
    unsigned outCount=0,outRead=0,outWrite=0;
    double corePhase=0;
    unsigned ticks=0;
    int requestedBank=-1;
    // Reboots and destruction are serviced off the audio thread, including
    // when no ModuleWidget exists (headless rendering).
    std::atomic<bool> maintenanceStop{false};
    std::thread maintenance;
    std::atomic<CoreEngine*> preparedEngine{nullptr},retiredEngine{nullptr};

    static CoreEngine *newEngine(){std::random_device random;return core_engine_create((uint64_t(random())<<32)|random());}
    Ectocore(){
        config(NUM_PARAMS,NUM_INPUTS,NUM_OUTPUTS,NUM_LIGHTS);
        const char *knobs[]={"Break / effects probability","Grimoire / effects bank","Amen / sequence","Amen random / CV range","Sample / bank"};
        const char *buttons[]={"Mode","Clock multiplier","Bank","Tap tempo / shift"};
        for(int i=0;i<5;++i)configParam(i,0,1,i==JUMP?.5f:0,knobs[i]);
        for(int i=0;i<4;++i){configButton(MODE+i,buttons[i]);getParamQuantity(MODE+i)->randomizeEnabled=false;}
        configInput(AMEN_CV,"Amen");configInput(BREAK_CV,"Break");configInput(SAMPLE_CV,"Sample");configInput(CLOCK_IN,"Clock (2 PPQN)");
        configOutput(LEFT,"Left audio");configOutput(RIGHT,"Right audio");configOutput(TRIGGER,"Transient trigger");configOutput(CLOCK_OUT,"Clock");
        engine=newEngine();if(!engine)throw Exception("Cannot allocate Ectocore engine");
        core_engine_get_state(engine,&initial);uiState=initial;
        resampler.setQuality(8);resampler.setRates(CORE_RATE,CORE_RATE);
        maintenance=std::thread([this]{
            while(!maintenanceStop){
                if(auto *old=retiredEngine.exchange(nullptr))core_engine_destroy(old);
                if(!preparedEngine.load()&&resetRequested.exchange(false)){
                    auto *next=newEngine();if(next)preparedEngine.store(next);
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        });
    }
    ~Ectocore(){maintenanceStop=true;maintenance.join();core_engine_destroy(engine);core_engine_destroy(preparedEngine.exchange(nullptr));core_engine_destroy(retiredEngine.exchange(nullptr));delete active;}
    std::string getFolder(){std::lock_guard<std::mutex> lock(pathMutex);return folder;}
    void choose(const std::string &path){
        {std::lock_guard<std::mutex> lock(pathMutex);folder=path;}
        storage.load(path,selectedBank.load(),true);
    }
    void collectUi(){
        CoreState state;while(snapshots.pop(state))uiState=state;
    }
    static bool read(void *user,unsigned b,unsigned s,unsigned v,uint64_t pos,void *out,size_t n){auto *m=static_cast<Ectocore*>(user);return m->storage.read(m->active,b,s,v,pos,out,n);}
    void onSampleRateChange(const SampleRateChangeEvent &e) override {
        Module::onSampleRateChange(e);resampler.setRates(CORE_RATE,int(e.sampleRate));outCount=outRead=outWrite=0;corePhase=0;
    }
    void onReset(const ResetEvent &e) override {Module::onReset(e);latch=0;resetRequested=true;}
    void process(const ProcessArgs &args) override {
        if(auto *fresh=preparedEngine.exchange(nullptr)){
            auto *old=engine;engine=fresh;retiredEngine.store(old);latch=0;resetRequested=false;
            for(int p=MODE;p<=TAP;++p)params[p].setValue(0);
            if(active){core_engine_set_catalogue(engine,active->library->banks.data(),read,this);core_engine_set_resident_bank(engine,active->index);CoreState s;core_engine_get_state(engine,&s);ecto::applySettings(*active->library,s);core_engine_update_settings(engine,&s);}
            outCount=outRead=outWrite=0;
        }
        ecto::Bank *bank;
        if(storage.retired.canPush()&&storage.ready.pop(bank)){
            if(active&&bank->library->generation<active->library->generation)storage.retired.push(bank);
            else {
                bool same=active&&active->library->generation==bank->library->generation;
                if(active)storage.retired.push(active);
                active=bank;storage.activeGeneration=bank->library->generation;
                core_engine_set_catalogue(engine,bank->library->banks.data(),read,this);
                if(restorePending){core_engine_set_state(engine,&restoreState);restorePending=false;}
                core_engine_set_resident_bank(engine,bank->index);
                if(!same&&(!hasPatchSettings||bank->library->importSettings)){auto s=initial;ecto::applySettings(*bank->library,s);core_engine_update_settings(engine,&s);}
                requestedBank=-1;
            }
        }
        Update update;
        while(edits.pop(update)){
            core_engine_update_settings(engine,&update.state);hasPatchSettings=true;
        }
        CoreControls controls{};
        for(unsigned i=0;i<5;++i)controls.knobs[i]=params[i].getValue();
        unsigned held=latch.load();for(unsigned i=0;i<4;++i)controls.buttons[i]=params[MODE+i].getValue()>.5f||(held&(1u<<i));
        for(unsigned i=0;i<3;++i){controls.cv[i]=inputs[i].getVoltage();controls.connected[i]=inputs[i].isConnected();}
        core_engine_controls(engine,&controls);
        bool edge=clockTrigger.process(inputs[CLOCK_IN].getVoltage(),.1f,1.f);
        if(edge)core_engine_clock(engine,true);else if(!clockTrigger.isHigh())core_engine_clock(engine,false);
        corePhase+=CORE_RATE/double(args.sampleRate);
        while(corePhase>=1){
            corePhase-=1;int16_t pcm[2];core_engine_process(engine,pcm);
            for(unsigned c=0;c<2;++c)resampleInput.samples[c]=float(pcm[c])/32768.f;
            int inFrames=1,outFrames=64;
            resampler.process(&resampleInput,&inFrames,resampleOutput,&outFrames);
            for(int i=0;i<outFrames&&outCount<256;++i){outputQueue[outWrite]=resampleOutput[i];outWrite=(outWrite+1)%256;++outCount;}
        }
        if(outCount){outputs[LEFT].setVoltage(outputQueue[outRead].samples[0]*5);outputs[RIGHT].setVoltage(outputQueue[outRead].samples[1]*5);outRead=(outRead+1)%256;--outCount;}
        else {outputs[LEFT].setVoltage(0);outputs[RIGHT].setVoltage(0);}
        CoreDisplay display{};core_engine_display(engine,&display);
        outputs[TRIGGER].setVoltage(display.trigger?10:0);outputs[CLOCK_OUT].setVoltage(display.clock?10:0);
        if((++ticks%44)==0){
            for(unsigned i=0;i<18;++i)for(unsigned c=0;c<3;++c)lights[RING+3*i+c].setBrightness(display.rgb[i][c]/117.f);
            for(unsigned i=0;i<4;++i)lights[MODES+i].setBrightness(display.mode[i]);
            lights[TAP_LIGHT].setBrightness(display.tap);lights[TRIGGER_LIGHT].setBrightness(display.trigger);lights[CLOCK_LIGHT].setBrightness(display.clock);
            selectedBank=display.bank;selectedSlot=display.slot;running=!display.stopped;
            if(display.requested_bank<16&&requestedBank!=int(display.requested_bank)){requestedBank=display.requested_bank;storage.requestedBank=requestedBank;}
            if(display.reboot_requested)resetRequested=true;
        }
        if(ticks%512==0){CoreState s;core_engine_get_state(engine,&s);snapshots.push(s);}
    }
    bool hasPatchSettings=false;
    json_t *dataToJson() override {
        auto *j=json_object();json_object_set_new(j,"schema",json_integer(1));json_object_set_new(j,"ezeptocore",json_boolean(ezeptocore.load()));
        auto path=getFolder();json_object_set_new(j,"folder",json_string(path.c_str()));CoreState s;core_engine_get_state(engine,&s);json_object_set_new(j,"state",ecto::stateJson(s));return j;
    }
    void dataFromJson(json_t *j) override {
        latch=0;for(int p=MODE;p<=TAP;++p)params[p].setValue(0);
        ezeptocore=!json_is_false(json_object_get(j,"ezeptocore"));restoreState=initial;
        restorePending=ecto::stateFromJson(json_object_get(j,"state"),restoreState);hasPatchSettings=restorePending;
        if(restorePending&&!core_engine_set_state(engine,&restoreState))restorePending=hasPatchSettings=false;
        auto *path=json_string_value(json_object_get(j,"folder"));if(path){
            {std::lock_guard<std::mutex> lock(pathMutex);folder=path;}
            storage.load(path,restorePending?restoreState.bank:0);
        }
    }
};

static NVGcolor ink(bool eze){return eze?nvgRGB(224,203,135):nvgRGB(223,247,255);}
struct CoreKnob : app::Knob {
    bool eze=false;
    CoreKnob(){box.size=mm2px(Vec(12,12));minAngle=-.83f*M_PI;maxAngle=.83f*M_PI;smooth=false;}
    void draw(const DrawArgs &a) override {
        auto *q=getParamQuantity();float v=q?q->getScaledValue():0;float r=box.size.x/2,c=r;
        nvgBeginPath(a.vg);nvgCircle(a.vg,c,c,r-1);nvgFillPaint(a.vg,nvgLinearGradient(a.vg,0,0,box.size.x,box.size.y,eze?nvgRGB(239,215,153):nvgRGB(241,246,246),nvgRGB(65,77,86)));nvgFill(a.vg);
        nvgBeginPath(a.vg);nvgCircle(a.vg,c,c,r*.79f);nvgFillColor(a.vg,eze?nvgRGB(12,15,21):nvgRGB(10,107,153));nvgFill(a.vg);
        float angle=minAngle+(maxAngle-minAngle)*v;
        nvgBeginPath(a.vg);nvgMoveTo(a.vg,c+sin(angle)*r*.3f,c-cos(angle)*r*.3f);nvgLineTo(a.vg,c+sin(angle)*r*.72f,c-cos(angle)*r*.72f);nvgStrokeWidth(a.vg,2);nvgStrokeColor(a.vg,nvgRGB(251,249,233));nvgStroke(a.vg);
    }
};
struct CoreButton : app::Switch {
    Ectocore *owner=nullptr;unsigned button=0;bool eze=false;
    CoreButton(){momentary=true;box.size=mm2px(Vec(4.8,4.8));}
    void draw(const DrawArgs &a) override {
        bool held=owner&&(owner->latch.load()&(1u<<button));bool on=held||(getParamQuantity()&&getParamQuantity()->getValue()>.5f);float r=box.size.x/2;
        nvgBeginPath(a.vg);nvgCircle(a.vg,r,r,r-.5);nvgFillColor(a.vg,ink(eze));nvgFill(a.vg);
        nvgBeginPath(a.vg);nvgCircle(a.vg,r,r,r*.74f);nvgFillColor(a.vg,on?nvgRGB(80,225,255):eze?nvgRGB(31,33,37):nvgRGB(213,222,227));nvgFill(a.vg);
        if(held){nvgBeginPath(a.vg);nvgCircle(a.vg,r,r,r);nvgStrokeColor(a.vg,nvgRGB(255,186,59));nvgStrokeWidth(a.vg,2);nvgStroke(a.vg);}
    }
    void appendContextMenu(ui::Menu *menu) override {
        if(!owner)return;menu->addChild(new ui::MenuSeparator);
        menu->addChild(createCheckMenuItem("Temporarily hold", "",[this]{return (owner->latch.load()&(1u<<button))!=0;},[this]{owner->latch.fetch_xor(1u<<button);}));
    }
};
struct CorePort : app::PortWidget {
    bool eze=false;
    CorePort(){box.size=mm2px(Vec(7.6,7.6));}
    void draw(const DrawArgs &a) override {
        float r=box.size.x/2;nvgBeginPath(a.vg);
        for(unsigned i=0;i<6;++i){float angle=(i+.5f)*M_PI/3;float x=r+cos(angle)*(r-.4f),y=r+sin(angle)*(r-.4f);if(!i)nvgMoveTo(a.vg,x,y);else nvgLineTo(a.vg,x,y);}nvgClosePath(a.vg);nvgFillColor(a.vg,eze?nvgRGB(213,183,116):nvgRGB(181,196,204));nvgFill(a.vg);
        nvgBeginPath(a.vg);nvgCircle(a.vg,r,r,r*.72f);nvgFillColor(a.vg,nvgRGB(234,232,211));nvgFill(a.vg);
        nvgBeginPath(a.vg);nvgCircle(a.vg,r,r,r*.53f);nvgFillColor(a.vg,nvgRGB(14,22,29));nvgFill(a.vg);
    }
};
struct Faceplate : widget::Widget {
    Ectocore *owner=nullptr;bool eze=false;
    std::shared_ptr<window::Font> ectocoreFont;
    Faceplate(){ectocoreFont=APP->window->loadFont(asset::plugin(pluginInstance,"res/odin-rounded.regular.otf"));}
    void draw(const DrawArgs &a) override {
        auto vg=a.vg;nvgSave(vg);nvgScale(vg,box.size.x/40.64f,box.size.y/128.5f);
        nvgBeginPath(vg);nvgRect(vg,0,0,40.64f,128.5f);nvgFillPaint(vg,nvgLinearGradient(vg,0,0,40,128,eze?nvgRGB(24,31,42):nvgRGB(7,100,150),eze?nvgRGB(6,10,19):nvgRGB(4,44,77)));nvgFill(vg);
        nvgStrokeColor(vg,ink(eze));nvgStrokeWidth(vg,.24f);nvgBeginPath(vg);nvgRect(vg,.45f,.45f,39.74f,127.6f);nvgStroke(vg);
        for(float y:{8.f,9.f,120.f,121.f}){nvgBeginPath(vg);nvgMoveTo(vg,.5f,y);nvgLineTo(vg,40.1f,y);nvgStroke(vg);}
        auto font=(!eze&&ectocoreFont)?ectocoreFont:APP->window->uiFont;
        nvgFontFaceId(vg,font->handle);
        auto label=[&](float x,float y,const char *s,float size=2.5f){nvgFontSize(vg,size);nvgTextAlign(vg,NVG_ALIGN_CENTER|NVG_ALIGN_MIDDLE);nvgFillColor(vg,ink(eze));nvgText(vg,x,y,s,nullptr);};
        label(23,4.5f,eze?"EZEPTOCORE":"Ectocore",eze?3.6f:4.5f);
        nvgStrokeColor(vg,eze?nvgRGBA(191,160,94,110):nvgRGBA(53,178,217,90));
        nvgBeginPath(vg);nvgCircle(vg,20.32f,24,14.2f);nvgCircle(vg,20.32f,24,13.7f);nvgStroke(vg);
        label(20.32f,38,eze?"AMEN":"Break",3);
        label(8.5f,57,eze?"BREAK":"Amen",2.9f);label(31.3f,51.5f,eze?"EFFECTS":"Grimoire",2.6f);
        label(31.3f,67.2f,eze?"JUMP":"Random",2.4f);label(8.5f,80,eze?"TUNNEL":"Sample",2.9f);
        label(21,82,"BANK",2);label(32,82,"TAP ↑",2.4f);
        label(26,92,"MODE",2.1f);label(36,92,eze?"X/":"MULT",2.1f);
        const char *runes[]={"I","II","III","IV","V","VI","VII"};
        if(eze)for(unsigned i=0;i<7;++i){float angle=-2.45f+i*4.9f/6;label(31.3f+sin(angle)*6.3f,43-cos(angle)*6.3f,runes[i],1.9f);}
        for(unsigned i=0;i<4;++i)label(3.3f+4.9f*i,85.2f,i==0?"K":i==1?"S":i==2?"T":"?",2.1f);
        const char *upper[]={"AMEN","BREAK",eze?"SLICE":"TRIG","CLK"};
        const char *lower[]={"CLK IN",eze?"TUNNEL":"SAMPLE","L OUT","R OUT"};
        for(unsigned i=0;i<4;++i){label(5+10.2f*i,105,upper[i],2.1f);label(5+10.2f*i,118,lower[i],2.1f);}
        nvgBeginPath(vg);nvgRoundedRect(vg,19.3f,55,2.5f,17,.6f);nvgFillColor(vg,ink(eze));nvgFill(vg);nvgBeginPath(vg);nvgRect(vg,20,56,1.1f,15);nvgFillColor(vg,nvgRGB(10,13,20));nvgFill(vg);label(20.5f,53,"SD",1.9f);
        label(24,124,"INFINITE DIGITS",2.5f);label(24,126.6f,eze?"MANECO LABS":"TOADSTOOL TECH",1.9f);
        if(owner&&owner->running.load()){nvgBeginPath(vg);nvgCircle(vg,20.5f,74,1);nvgFillColor(vg,nvgRGB(105,229,230));nvgFill(vg);}
        nvgRestore(vg);
    }
};
struct EctocoreWidget : app::ModuleWidget {
    Faceplate *face;std::array<CoreKnob*,5> knobs{};std::array<CoreButton*,4> buttons{};std::vector<CorePort*> ports;
    std::array<widget::SvgWidget*,7> runes{};
    bool lastEze=false;bool wasFocused=true;
    EctocoreWidget(Ectocore *m){
        setModule(m);box.size=Vec(8*RACK_GRID_WIDTH,RACK_GRID_HEIGHT);
        face=new Faceplate;face->owner=m;face->box.size=box.size;addChild(face);
        for(unsigned i=0;i<7;++i){float a=-2.45f+i*4.9f/6;auto *r=new widget::SvgWidget;
            r->setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/rune"+std::to_string(i+1)+".svg")));
            auto *scale=new widget::TransformWidget;scale->scale(mm2px(2.7f)/16.f);scale->addChild(r);scale->box.pos=mm2px(Vec(31.3f+sin(a)*6.3f-1.35f,43-cos(a)*6.3f-1.35f));addChild(scale);runes[i]=r;}
        const Vec positions[]={Vec(20.32f,24),Vec(31.3f,43),Vec(8.5f,50),Vec(31.3f,61),Vec(8.5f,73)};
        for(unsigned i=0;i<5;++i){auto *k=createParamCentered<CoreKnob>(mm2px(positions[i]),m,i);float size=i==0?16:i==2||i==4?12:7.5f;k->box.size=mm2px(Vec(size,size));k->box.pos=mm2px(positions[i])-k->box.size.div(2);knobs[i]=k;addParam(k);}
        const Vec bp[]={Vec(26,87),Vec(36,87),Vec(20,78),Vec(31.3f,76.5f)};
        for(unsigned i=0;i<4;++i){auto *b=createParamCentered<CoreButton>(mm2px(bp[i]),m,Ectocore::MODE+i);b->owner=m;b->button=i;if(i==3){b->box.size=mm2px(Vec(8,8));b->box.pos=mm2px(bp[i])-b->box.size.div(2);}buttons[i]=b;addParam(b);}
        auto in=[&](float x,float y,int id){auto *p=createInputCentered<CorePort>(mm2px(Vec(x,y)),m,id);ports.push_back(p);addInput(p);};
        auto out=[&](float x,float y,int id){auto *p=createOutputCentered<CorePort>(mm2px(Vec(x,y)),m,id);ports.push_back(p);addOutput(p);};
        in(5,99,Ectocore::AMEN_CV);in(15.2f,99,Ectocore::BREAK_CV);out(25.4f,99,Ectocore::TRIGGER);out(35.6f,99,Ectocore::CLOCK_OUT);
        in(5,112,Ectocore::CLOCK_IN);in(15.2f,112,Ectocore::SAMPLE_CV);out(25.4f,112,Ectocore::LEFT);out(35.6f,112,Ectocore::RIGHT);
        for(unsigned i=0;i<16;++i){float a=-2.75f+i*5.5f/15;addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(mm2px(Vec(20.32f+sin(a)*11.5f,24-cos(a)*11.5f)),m,Ectocore::RING+3*i));}
        addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(mm2px(Vec(2.7f,13.5f)),m,Ectocore::MODIFIERS));
        addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(mm2px(Vec(37.7f,13.5f)),m,Ectocore::MODIFIERS+3));
        for(unsigned i=0;i<4;++i)addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(3.3f+4.9f*i,89)),m,Ectocore::MODES+i));
        addChild(createLightCentered<SmallLight<WhiteLight>>(mm2px(Vec(31.3f,83.7f)),m,Ectocore::TAP_LIGHT));
        addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(28,107)),m,Ectocore::TRIGGER_LIGHT));
        addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(38,107)),m,Ectocore::CLOCK_LIGHT));
        addChild(createWidget<ScrewSilver>(mm2px(Vec(2,1))));addChild(createWidget<ScrewSilver>(mm2px(Vec(2,123))));
        setAppearance(m?m->ezeptocore.load():true);
    }
    void setAppearance(bool eze) {
        for(auto *r:runes)r->visible=!eze;
        face->eze=eze;for(auto *k:knobs)k->eze=eze;for(auto *b:buttons)b->eze=eze;for(auto *p:ports)p->eze=eze;
        if(eze!=lastEze){
            Vec top=mm2px(Vec(20.32f,24)),side=mm2px(Vec(8.5f,50));
            for(unsigned i:{0u,2u}){bool big=(eze?i==2:i==0);knobs[i]->box.size=mm2px(Vec(big?16:12,big?16:12));knobs[i]->box.pos=(big?top:side)-knobs[i]->box.size.div(2);}lastEze=eze;
        }
    }
    void step() override {
        auto *m=getModule<Ectocore>();
        if(m){m->collectUi();bool focused=glfwGetWindowAttrib(APP->window->win,GLFW_FOCUSED);if(wasFocused&&!focused){m->latch=0;for(int p=Ectocore::MODE;p<=Ectocore::TAP;++p)m->params[p].setValue(0);}wasFocused=focused;}
        setAppearance(m?m->ezeptocore.load():true);
        ModuleWidget::step();
    }
    void appendContextMenu(ui::Menu *menu) override {
        auto *m=getModule<Ectocore>();if(!m)return;m->collectUi();menu->addChild(new ui::MenuSeparator);
        menu->addChild(createMenuLabel(m->storage.status()));
        menu->addChild(createMenuItem("Choose sample folder…","",[m]{char *path=osdialog_file(OSDIALOG_OPEN_DIR,nullptr,nullptr,nullptr);if(path){m->choose(path);std::free(path);}}));
        menu->addChild(createMenuItem("Reload sample folder","",[m]{auto path=m->getFolder();if(!path.empty())m->storage.load(path,m->selectedBank.load());}));
        menu->addChild(createMenuItem("Import settings from sample folder","",[m]{auto path=m->getFolder();if(!path.empty())m->storage.load(path,m->selectedBank.load(),true);}));
        menu->addChild(createMenuItem("Reveal sample folder","",[m]{auto path=m->getFolder();if(!path.empty())system::openDirectory(path);}));
        menu->addChild(createCheckMenuItem("Ectocore appearance","",[m]{return !m->ezeptocore.load();},[m]{m->ezeptocore=!m->ezeptocore.load();}));
        menu->addChild(createMenuItem("Release held buttons","",[m]{m->latch=0;}));
        menu->addChild(createMenuItem("Reboot module","",[m]{m->resetRequested=true;}));
        menu->addChild(createMenuLabel("Bank "+std::to_string(m->selectedBank+1)+" · Sample "+std::to_string(m->selectedSlot+1)));
        menu->addChild(createMenuLabel("Companion cache: 64 MiB · misses "+std::to_string(m->storage.misses.load())));
        menu->addChild(createSubmenuItem("Device settings","",[m](ui::Menu *sub){
            auto toggle=[&](const char *label,bool CoreState::*field){sub->addChild(createCheckMenuItem(label,"",[m,field]{return m->uiState.*field;},[m,field]{auto s=m->uiState;s.*field=!(s.*field);m->edits.push({s});}));};
            toggle("Stop when clock stops",&CoreState::clock_stop);toggle("Clock output is a trigger",&CoreState::clock_trigger);toggle("Clock follows slices",&CoreState::clock_slice);
            const char *names[]={"Amen CV bipolar","Break CV bipolar","Sample CV bipolar"};
            for(unsigned i=0;i<3;++i)sub->addChild(createCheckMenuItem(names[i],"",[m,i]{return m->uiState.bipolar[i];},[m,i]{auto s=m->uiState;s.bipolar[i]=!s.bipolar[i];m->edits.push({s});}));
            sub->addChild(createSubmenuItem("Amen CV behavior","",[m](ui::Menu *mnu){const char *names[]={"Jump","Repeat","Split"};for(unsigned i=0;i<3;++i)mnu->addChild(createCheckMenuItem(names[i],"",[m,i]{return m->uiState.amen_behavior==i;},[m,i]{auto s=m->uiState;s.amen_behavior=i;m->edits.push({s});}));}));
            sub->addChild(createCheckMenuItem("Sample CV: 1 V/oct","",[m]{return m->uiState.sample_mapping==1;},[m]{auto s=m->uiState;s.sample_mapping=1-s.sample_mapping;m->edits.push({s});}));
            sub->addChild(createSubmenuItem("Reset input assignment","",[m](ui::Menu *mnu){const char *names[]={"None","Amen","Break","Sample","Clock"};for(int i=-1;i<4;++i)mnu->addChild(createCheckMenuItem(names[i+1],"",[m,i]{return m->uiState.reset_input==i;},[m,i]{auto s=m->uiState;s.reset_input=i;m->edits.push({s});}));}));
            sub->addChild(createSubmenuItem("LED brightness","",[m](ui::Menu *mnu){for(unsigned v:{0u,25u,50u,75u,100u})mnu->addChild(createCheckMenuItem(std::to_string(v)+"%","",[m,v]{return m->uiState.brightness==v;},[m,v]{auto s=m->uiState;s.brightness=v;m->edits.push({s});}));}));
        }));
        menu->addChild(createSubmenuItem("Grimoire / effect banks","",[m](ui::Menu *sub){
            const char *effects[]={"Saturate","Shaper","Fuzz","Bitcrush","Time stretch","Delay","Comb","Beat repeat","Tighten","Expand / reverb","Pan","Scratch","Filter","Repitch","Reverse","Tape stop"};
            for(unsigned r=0;r<7;++r)sub->addChild(createSubmenuItem("Bank "+std::to_string(r+1),"",[m,r,effects=std::array<std::string,16>{effects[0],effects[1],effects[2],effects[3],effects[4],effects[5],effects[6],effects[7],effects[8],effects[9],effects[10],effects[11],effects[12],effects[13],effects[14],effects[15]}](ui::Menu *mnu){for(unsigned f=0;f<16;++f)mnu->addChild(createCheckMenuItem(effects[f],"",[m,r,f]{return m->uiState.runes[r][f];},[m,r,f]{auto s=m->uiState;s.runes[r][f]=!s.runes[r][f];m->edits.push({s});}));}));
        }));
    }
};
Model *modelEzeptocore=createModel<Ectocore,EctocoreWidget>("EZEPTOCORE");
void init(Plugin *p){pluginInstance=p;p->addModel(modelEzeptocore);}
