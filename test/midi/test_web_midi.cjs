const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const root = path.join(__dirname, '../..');
let now = 0, nextTimer = 0;
const timers = new Map(), sent = [];
const input = {name:'zeptocore', id:'input', state:'connected', onmidimessage:null};
const output = {name:'zeptocore', id:'output', state:'connected', send(data) {sent.push(data);}};
const access = {inputs:new Map([['in',input]]), outputs:new Map([['out',output]])};
const context = vm.createContext({
    console:{log(){},error(){}}, Date:class extends Date {static now() {return now;}},
    location:{pathname:'/midi-test'}, addEventListener(){},
    setInterval(fn){timers.set(++nextTimer,fn);return nextTimer;}, clearInterval(id){timers.delete(id);},
    setTimeout(){}, clearTimeout(){},
    document:{cookie:'',getElementById(){return {style:{},innerHTML:'',scrollHeight:0,clientHeight:0,scrollTop:0};}},
    navigator:{requestMIDIAccess:async()=>access},
    Vue:function(options){return {...options.data,...options.methods};},
});
context.window = context;
vm.runInContext(fs.readFileSync(path.join(root,'core/src/server/static/core-midi.js'),'utf8'),context);
vm.runInContext(fs.readFileSync(path.join(root,'core/src/server/static/app.js'),'utf8'),context);
const text = data => Buffer.from(data.slice(1,-1)).toString();
const reply = value => input.onmidimessage({data:[0xf0,...Buffer.from(value),0xf7]});
(async()=>{
    context.listMidiPorts(); await new Promise(resolve=>setImmediate(resolve));
    assert.equal(sent.length,1); assert.equal(text(sent[0]),'core_cmd=1,hello'); assert.equal(timers.size,1);
    access.onstatechange(); assert.equal(sent.length,1);
    context.midiResetDevice(); assert.equal(sent.length,1); // held until negotiated
    reply('core_caps=1'); assert.equal(text(sent.at(-1)),'core_cmd=1,version');
    reply('version=v-next'); assert.equal(context.app.deviceVersion,'v-next');
    context.midiResetDevice(); assert.equal(text(sent.at(-1)),'core_cmd=1,bootloader');
    output.state='disconnected';access.onstatechange();
    assert.equal(timers.size,0);assert.equal(input.onmidimessage,null);
    output.state='connected';access.onstatechange();assert.equal(timers.size,1);
    assert.equal(text(sent.at(-1)),'core_cmd=1,hello');
    now=500;for(const fn of timers.values())fn();assert.equal(text(sent.at(-1)),'core_cmd=1,hello');
    now=1000;for(const fn of timers.values())fn();assert.deepEqual(Array.from(sent.at(-1)),[0xb0,1,0]);
    const before=sent.length;context.midiResetDevice();assert.equal(sent.length,before);
    reply('version=v-old');context.midiResetDevice();assert.deepEqual(Array.from(sent.at(-1)),[0xb0,0,0]);
    assert.equal(timers.size,1);
    console.log('Web manager connects once, negotiates, routes controls and clears state on output disconnect');
})().catch(error=>{console.error(error);process.exitCode=1;});
