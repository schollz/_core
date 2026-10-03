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
            CoreState previous;core_engine_get_state(engine,&previous);
            auto *old=engine;engine=fresh;retiredEngine.store(old);latch=0;resetRequested=false;
            for(int p=MODE;p<=TAP;++p)params[p].setValue(0);
            if(active){core_engine_set_catalogue(engine,active->library->banks.data(),read,this);core_engine_set_resident_bank(engine,active->index);CoreState s;core_engine_get_state(engine,&s);ecto::applySettings(*active->library,s);core_engine_update_settings(engine,&s);}
            CoreState reboot;core_engine_get_state(engine,&reboot);reboot.start_tempo=previous.start_tempo;
            core_engine_update_settings(engine,&reboot);core_engine_apply_start_tempo(engine);
            outCount=outRead=outWrite=0;
        }
        ecto::Bank *bank;
        if(storage.retired.canPush()&&storage.ready.pop(bank)){
            if(active&&bank->library->generation<active->library->generation)storage.retired.push(bank);
            else {
                bool same=active&&active->library->generation==bank->library->generation;
                bool startup=!active||active->library->root!=bank->library->root||restorePending;
                if(active)storage.retired.push(active);
                active=bank;storage.activeGeneration=bank->library->generation;
                core_engine_set_catalogue(engine,bank->library->banks.data(),read,this);
                if(restorePending){core_engine_set_state(engine,&restoreState);restorePending=false;}
                core_engine_set_resident_bank(engine,bank->index);
                if(!same&&(!hasPatchSettings||bank->library->importSettings)){auto s=initial;ecto::applySettings(*bank->library,s);core_engine_update_settings(engine,&s);hasPatchSettings=true;}
                if(startup)core_engine_apply_start_tempo(engine);
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
        if(restorePending)restoreState.tempo=start_tempo_resolve(restoreState.start_tempo,restoreState.tempo);
        if(restorePending&&!core_engine_set_state(engine,&restoreState))restorePending=hasPatchSettings=false;
        auto *path=json_string_value(json_object_get(j,"folder"));if(path){
            {std::lock_guard<std::mutex> lock(pathMutex);folder=path;}
            storage.load(path,restorePending?restoreState.bank:0);
        }
    }
};

struct StartTempoField : ui::TextField {
    Ectocore *module=nullptr;
    void onAction(const ActionEvent &e) override {
        uint16_t bpm=0;
        if(start_tempo_parse(text.data(),text.size(),&bpm)&&bpm){
            auto state=module->uiState;state.start_tempo=bpm;
            if(module->edits.push({state}))module->uiState=state;
        }
        setText(module->uiState.start_tempo?std::to_string(module->uiState.start_tempo):"130");
        e.consume(this);
    }
};

static NVGcolor ink(bool eze){return eze?nvgRGB(255,203,133):nvgRGB(223,247,255);}
struct CoreKnob : app::Knob {
    bool eze=false;
    CoreKnob(){box.size=mm2px(Vec(12,12));minAngle=-.83f*M_PI;maxAngle=.83f*M_PI;smooth=false;}
    void draw(const DrawArgs &a) override {
        auto *q=getParamQuantity();float v=q?q->getScaledValue():0;float r=box.size.x/2,c=r;
        if(!eze){
            // Blue caps and pale trimmers match the hardware artwork.
            bool trimmer=box.size.x<mm2px(10.f);
            nvgBeginPath(a.vg);nvgCircle(a.vg,c,c+.7f,r-.4f);nvgFillColor(a.vg,nvgRGBA(10,24,51,125));nvgFill(a.vg);
            nvgBeginPath(a.vg);nvgCircle(a.vg,c,c,r-1);nvgFillPaint(a.vg,nvgLinearGradient(a.vg,0,0,box.size.x,box.size.y,nvgRGB(251,252,255),nvgRGB(174,187,204)));nvgFill(a.vg);
            nvgBeginPath(a.vg);nvgCircle(a.vg,c,c,r*.81f);nvgFillColor(a.vg,trimmer?nvgRGB(235,237,239):nvgRGB(59,112,212));nvgFill(a.vg);
            float angle=minAngle+(maxAngle-minAngle)*v;
            nvgBeginPath(a.vg);nvgMoveTo(a.vg,c+sin(angle)*r*.14f,c-cos(angle)*r*.14f);nvgLineTo(a.vg,c+sin(angle)*r*.75f,c-cos(angle)*r*.75f);nvgStrokeWidth(a.vg,trimmer?2.f:2.5f);nvgStrokeColor(a.vg,trimmer?nvgRGB(20,24,29):nvgRGB(255,255,255));nvgLineCap(a.vg,NVG_ROUND);nvgStroke(a.vg);
            return;
        }
        bool large=box.size.x>mm2px(14.f);
        nvgBeginPath(a.vg);nvgCircle(a.vg,c,c,r-.5f);nvgFillPaint(a.vg,nvgLinearGradient(a.vg,0,0,box.size.x,box.size.y,large?nvgRGB(247,245,232):nvgRGB(255,210,146),large?nvgRGB(177,179,174):nvgRGB(173,125,67)));nvgFill(a.vg);
        nvgBeginPath(a.vg);nvgCircle(a.vg,c,c,large?r-1.3f:r*.79f);nvgFillColor(a.vg,nvgRGB(13,14,15));nvgFill(a.vg);
        float angle=minAngle+(maxAngle-minAngle)*v;
        nvgBeginPath(a.vg);nvgMoveTo(a.vg,c+sin(angle)*r*.3f,c-cos(angle)*r*.3f);nvgLineTo(a.vg,c+sin(angle)*r*.72f,c-cos(angle)*r*.72f);nvgStrokeWidth(a.vg,2);nvgStrokeColor(a.vg,nvgRGB(251,249,233));nvgStroke(a.vg);
    }
};
struct CoreButton : app::Switch {
    Ectocore *owner=nullptr;unsigned button=0;bool eze=false;
    CoreButton(){momentary=true;box.size=mm2px(Vec(4.8,4.8));}
    void draw(const DrawArgs &a) override {
        bool held=owner&&(owner->latch.load()&(1u<<button));bool on=held||(getParamQuantity()&&getParamQuantity()->getValue()>.5f);float r=box.size.x/2;
        nvgBeginPath(a.vg);nvgCircle(a.vg,r,r,r-.5);nvgFillColor(a.vg,eze?ink(true):nvgRGB(20,26,36));nvgFill(a.vg);
        nvgBeginPath(a.vg);nvgCircle(a.vg,r,r,r*(eze?.74f:.84f));nvgFillColor(a.vg,on?nvgRGB(80,225,255):eze?nvgRGB(31,33,37):nvgRGB(240,242,245));nvgFill(a.vg);
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
        for(unsigned i=0;i<6;++i){float angle=(i+.5f)*M_PI/3;float x=r+cos(angle)*(r-.4f),y=r+sin(angle)*(r-.4f);if(!i)nvgMoveTo(a.vg,x,y);else nvgLineTo(a.vg,x,y);}nvgClosePath(a.vg);nvgFillColor(a.vg,eze?nvgRGB(174,175,173):nvgRGB(181,196,204));nvgFill(a.vg);
        nvgBeginPath(a.vg);nvgCircle(a.vg,r,r,r*.72f);nvgFillColor(a.vg,eze?nvgRGB(213,213,206):nvgRGB(234,232,211));nvgFill(a.vg);
        nvgBeginPath(a.vg);nvgCircle(a.vg,r,r,r*.53f);nvgFillColor(a.vg,eze?nvgRGB(17,18,18):nvgRGB(14,22,29));nvgFill(a.vg);
    }
};

// The hardware's pale spiral lines are a diffuser over sixteen RGB sources.
// Reuse the panel's actual paths so illumination cannot cover the blue gaps.
struct BreakDiffuser : widget::TransparentWidget {
    Ectocore *owner=nullptr;
    std::array<app::ModuleLightWidget*,16> sources{};
    std::shared_ptr<window::Svg> artwork;
    std::vector<const NSVGpath*> paths;

    BreakDiffuser(){
        artwork=APP->window->loadSvg(asset::plugin(pluginInstance,"res/ectocore.svg"));
        if(artwork&&artwork->handle){
            box.size=artwork->getSize();
            for(auto *shape=artwork->handle->shapes;shape;shape=shape->next){
                if(std::strncmp(shape->id,"break-diffuser-",15)!=0)continue;
                for(auto *path=shape->paths;path;path=path->next)paths.push_back(path);
            }
        }
    }

    void trace(NVGcontext *vg) const {
        nvgBeginPath(vg);
        for(auto *path:paths){
            if(path->npts<4)continue;
            const float *p=path->pts;
            nvgMoveTo(vg,p[0],p[1]);
            for(int i=1;i+2<path->npts;i+=3){
                p=&path->pts[2*i];
                nvgBezierTo(vg,p[0],p[1],p[2],p[3],p[4],p[5]);
            }
            nvgClosePath(vg);
            // Each tagged shape is one solid, closed diffuser aperture.
            nvgPathWinding(vg,NVG_SOLID);
        }
    }

    void draw(const DrawArgs &a) override {
        // Frosted material must have headroom for the LEDs to illuminate it.
        // Painting over an almost-white surface hid normal playback at 50%.
        if(paths.empty())return;
        trace(a.vg);
        nvgFillColor(a.vg,nvgRGB(145,163,190));
        nvgFill(a.vg);
    }

    void drawLayer(const DrawArgs &a,int layer) override {
        if(layer!=1||!owner||paths.empty())return;
        auto *vg=a.vg;
        nvgSave(vg);
        nvgGlobalCompositeOperation(vg,NVG_SOURCE_OVER);
        trace(vg);
        nvgLineJoin(vg,NVG_ROUND);
        for(unsigned i=0;i<16;++i){
            if(!sources[i])continue;
            // These are the very same LED widgets used by the other skin.
            // Their step() supplies Rack's RGB mixing, brightness response,
            // and bypass handling even while their bulb graphics are hidden.
            NVGcolor light=sources[i]->color;
            float peak=std::max(light.r,std::max(light.g,light.b));
            float intensity=peak*light.a;
            if(intensity<.001f)continue;
            // Lift dim LEDs while retaining headroom through 100% brightness.
            // A hard exposure cap made the upper brightness settings identical.
            float level=std::sqrt(std::min(1.f,intensity));
            light.r/=peak;light.g/=peak;light.b/=peak;light.a=.94f*level;
            NVGcolor clear=light;clear.a=0;
            float angle=-2.75f+i*5.5f/15;
            Vec center((20.32f+std::sin(angle)*11.5f)*box.size.x/40.64f,
                       (21.4968f-std::cos(angle)*11.5f)*box.size.y/128.5f);
            nvgFillPaint(vg,nvgRadialGradient(vg,center.x,center.y,mm2px(.8f),mm2px(8.2f),light,clear));
            nvgFill(vg);
            // Bloom follows the same line edges and honors Rack's halo setting.
            if(settings::haloBrightness>0){
                light.a=.14f*level*settings::haloBrightness;
                nvgStrokePaint(vg,nvgRadialGradient(vg,center.x,center.y,0,mm2px(8.2f),light,clear));
                nvgStrokeWidth(vg,mm2px(.45f));
                nvgStroke(vg);
            }
        }
        nvgRestore(vg);
    }
};

struct EctocoreWidget : app::ModuleWidget {
    app::SvgPanel *face;std::array<CoreKnob*,5> knobs{};std::array<CoreButton*,4> buttons{};std::vector<CorePort*> ports;
    app::SvgPanel *ectocorePanel;
    BreakDiffuser *breakDiffuser;
    std::array<app::ModuleLightWidget*,25> panelLights{};
    SmallLight<WhiteLight> *tapLight;
    std::array<widget::Widget*,2> screws{};
    bool lastEze=false;bool appearanceInitialized=false;bool wasFocused=true;
    EctocoreWidget(Ectocore *m){
        setModule(m);box.size=Vec(8*RACK_GRID_WIDTH,RACK_GRID_HEIGHT);
        face=createPanel(asset::plugin(pluginInstance,"res/ezeptocore.svg"));addChild(face);
        ectocorePanel=createPanel(asset::plugin(pluginInstance,"res/ectocore.svg"));addChild(ectocorePanel);
        breakDiffuser=new BreakDiffuser;breakDiffuser->owner=m;addChild(breakDiffuser);
        const Vec positions[]={Vec(20.32f,24),Vec(31.3f,43),Vec(8.5f,50),Vec(31.3f,61),Vec(8.5f,73)};
        for(unsigned i=0;i<5;++i){auto *k=createParamCentered<CoreKnob>(mm2px(positions[i]),m,i);float size=i==0?16:i==2||i==4?12:7.5f;k->box.size=mm2px(Vec(size,size));k->box.pos=mm2px(positions[i])-k->box.size.div(2);knobs[i]=k;addParam(k);}
        const Vec bp[]={Vec(26,87),Vec(36,87),Vec(20,78),Vec(31.3f,76.5f)};
        for(unsigned i=0;i<4;++i){auto *b=createParamCentered<CoreButton>(mm2px(bp[i]),m,Ectocore::MODE+i);b->owner=m;b->button=i;if(i==3){b->box.size=mm2px(Vec(8,8));b->box.pos=mm2px(bp[i])-b->box.size.div(2);}buttons[i]=b;addParam(b);}
        auto in=[&](float x,float y,int id){auto *p=createInputCentered<CorePort>(mm2px(Vec(x,y)),m,id);ports.push_back(p);addInput(p);};
        auto out=[&](float x,float y,int id){auto *p=createOutputCentered<CorePort>(mm2px(Vec(x,y)),m,id);ports.push_back(p);addOutput(p);};
        in(5,99,Ectocore::AMEN_CV);in(15.2f,99,Ectocore::BREAK_CV);out(25.4f,99,Ectocore::TRIGGER);out(35.6f,99,Ectocore::CLOCK_OUT);
        in(5,112,Ectocore::CLOCK_IN);in(15.2f,112,Ectocore::SAMPLE_CV);out(25.4f,112,Ectocore::LEFT);out(35.6f,112,Ectocore::RIGHT);
        for(unsigned i=0;i<18;++i){panelLights[i]=createLightCentered<SmallLight<RedGreenBlueLight>>(Vec(),m,Ectocore::RING+3*i);addChild(panelLights[i]);}
        for(unsigned i=0;i<16;++i)breakDiffuser->sources[i]=panelLights[i];
        for(unsigned i=0;i<4;++i){panelLights[18+i]=createLightCentered<SmallLight<GreenLight>>(Vec(),m,Ectocore::MODES+i);addChild(panelLights[18+i]);}
        tapLight=createLightCentered<SmallLight<WhiteLight>>(Vec(),m,Ectocore::TAP_LIGHT);panelLights[22]=tapLight;addChild(tapLight);
        panelLights[23]=createLightCentered<SmallLight<GreenLight>>(Vec(),m,Ectocore::TRIGGER_LIGHT);addChild(panelLights[23]);
        panelLights[24]=createLightCentered<SmallLight<GreenLight>>(Vec(),m,Ectocore::CLOCK_LIGHT);addChild(panelLights[24]);
        for(auto &s:screws){s=createWidget<ScrewSilver>(Vec());addChild(s);}
        setAppearance(m?m->ezeptocore.load():true);
    }
    void setAppearance(bool eze) {
        if(appearanceInitialized&&eze==lastEze)return;
        face->visible=eze;ectocorePanel->visible=!eze;breakDiffuser->visible=!eze;
        for(auto *k:knobs)k->eze=eze;for(auto *b:buttons)b->eze=eze;for(auto *p:ports)p->eze=eze;
        // EZEPTOCORE positions are pixels in its fitted 120 x 380 SVG.
        // Ectocore uses hardware drill coordinates in millimeters.
        auto point=[&](Vec p){return eze?p:Vec(p.x*box.size.x/40.64f,p.y*box.size.y/128.5f);};
        auto place=[&](widget::Widget *w,Vec mm){w->box.pos=point(mm)-w->box.size.div(2);};
        const Vec ezeKnobs[]={Vec(25.953f,144.512f),Vec(92.277f,125.473f),Vec(59.371f,68.786f),Vec(92.232f,177.712f),Vec(25.953f,211.039f)};
        const float ezeKnobSizes[]={9.f,7.5f,15.4f,9.f,9.f};
        const Vec ectoKnobs[]={Vec(20.32f,21.4968f),Vec(31.78f,40.5649f),Vec(8.452f,47.1488f),Vec(31.78f,58.2178f),Vec(8.92f,69.6348f)};
        for(unsigned i=0;i<5;++i){
            bool big=i==(eze?Ectocore::AMEN:Ectocore::BREAK);
            float size=eze?ezeKnobSizes[i]:big?16.f:(i==Ectocore::GRIMOIRE||i==Ectocore::JUMP)?7.f:12.f;
            knobs[i]->box.size=mm2px(Vec(size,size));place(knobs[i],eze?ezeKnobs[i]:ectoKnobs[i]);
        }
        const Vec ezeButtons[]={Vec(73.996f,263.609f),Vec(103.5f,263.488f),Vec(58.831f,228.035f),Vec(92.147f,219.482f)};
        const Vec ectoButtons[]={Vec(25.684f,87.6688f),Vec(35.844f,87.6688f),Vec(20.477f,75.3498f),Vec(31.78f,72.9369f)};
        for(unsigned i=0;i<4;++i){float size=i==3?(eze?6.f:9.1f):(eze?5.4f:5.7f);buttons[i]->box.size=mm2px(Vec(size,size));place(buttons[i],eze?ezeButtons[i]:ectoButtons[i]);}
        const float ectoPortX[]={4.856f,15.27f,25.684f,35.844f};
        const Vec ezePorts[]={Vec(14.877f,293.155f),Vec(44.578f,293.282f),Vec(74.22f,293.642f),Vec(103.769f,293.642f),Vec(14.868f,334.233f),Vec(44.513f,334.246f),Vec(74.22f,334.355f),Vec(103.87f,334.355f)};
        for(unsigned i=0;i<ports.size();++i){float size=eze?7.f:7.6f;ports[i]->box.size=mm2px(Vec(size,size));place(ports[i],eze?ezePorts[i]:Vec(ectoPortX[i%4],i<4?98.1388f:112.561f));}
        const Vec ezeRing[]={Vec(44.249f,97.019f),Vec(35.397f,90.33f),Vec(29.713f,80.789f),Vec(27.427f,69.962f),Vec(28.667f,58.785f),Vec(33.829f,48.421f),Vec(42.583f,40.706f),Vec(53.689f,36.965f),Vec(65.874f,36.932f),Vec(76.425f,40.64f),Vec(84.955f,48.222f),Vec(90.148f,58.089f),Vec(91.749f,69.744f),Vec(89.462f,80.639f),Vec(83.932f,90.164f),Vec(75.83f,97.083f)};
        for(auto *light:panelLights){
            // The SVG supplies EZEPTOCORE's printed bezels; only the live lens
            // and halo are drawn over their exact centers.
            for(auto *child:light->children)child->visible=!eze;
            light->box.size=eze?Vec(9.47f,9.47f):tapLight->sw->box.size;
            light->bgColor=nvgRGBA(51,51,51,eze?0:255);
            light->borderColor=nvgRGBA(0,0,0,eze?0:53);
        }
        for(unsigned i=0;i<16;++i){panelLights[i]->visible=eze;float a=-2.75f+i*5.5f/15;place(panelLights[i],eze?ezeRing[i]:Vec(20.32f+sin(a)*11.5f,21.4968f-cos(a)*11.5f));}
        place(panelLights[16],eze?Vec(8.115f,39.377f):Vec(2.05793f,11.2714f));
        place(panelLights[17],eze?Vec(112.035f,38.931f):Vec(38.5247f,11.2714f));
        const float ectoModeX[]={3.7025f,8.66927f,13.6099f,18.5166f};
        const Vec ezeModes[]={Vec(9.957f,269.246f),Vec(24.554f,269.272f),Vec(39.398f,269.194f),Vec(54.242f,269.35f)};
        for(unsigned i=0;i<4;++i)place(panelLights[18+i],eze?ezeModes[i]:Vec(ectoModeX[i],89.9489f));
        // In Ectocore mode the Tap cap itself is the illuminated diffuser.
        tapLight->fb->visible=false;tapLight->box.size=eze?Vec(9.47f,9.47f):mm2px(Vec(7.6f,7.6f));
        tapLight->bgColor=nvgRGBA(0,0,0,0);tapLight->borderColor=nvgRGBA(0,0,0,0);
        place(panelLights[22],eze?Vec(91.833f,242.823f):Vec(31.78f,72.9369f));
        place(panelLights[23],eze?Vec(83.234f,314.196f):Vec(28.2283f,105.454f));
        place(panelLights[24],eze?Vec(112.794f,314.095f):Vec(38.389f,105.454f));
        if(eze){place(screws[0],Vec(21.f,9.5f));place(screws[1],Vec(21.f,370.4f));}
        else {place(screws[0],Vec(7.5f,3));place(screws[1],Vec(7.5f,125.5f));}
        lastEze=eze;appearanceInitialized=true;
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
        menu->addChild(createSubmenuItem("Device settings","",[m](ui::Menu *sub){
            sub->addChild(createSubmenuItem("Start tempo",m->uiState.start_tempo?std::to_string(m->uiState.start_tempo)+" BPM":"Default",[m](ui::Menu *mnu){
                mnu->addChild(createCheckMenuItem("Default (no override)","",[m]{return m->uiState.start_tempo==0;},[m]{auto s=m->uiState;s.start_tempo=0;if(m->edits.push({s}))m->uiState=s;}));
                mnu->addChild(createMenuLabel("Fixed BPM (30–300): enter to save"));
                auto *field=new StartTempoField;field->module=m;field->box.size=Vec(240,28);
                field->setText(m->uiState.start_tempo?std::to_string(m->uiState.start_tempo):"130");mnu->addChild(field);
                mnu->addChild(createMenuLabel("Applies on patch open or reboot."));
            }));
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
