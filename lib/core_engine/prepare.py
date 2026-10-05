#!/usr/bin/env python3
"""Build the desktop translation of the actual firmware (no DSP/control fork).

Only platform boundaries and the perpetual control loop are adapted here. Clang
then identifies every mutable static/global and its exact references, so the
generated C owns them in a CoreEngine rather than relying on process globals.
Generation is a development/build step; the distributed plugin needs no Python.
"""
from pathlib import Path
import argparse
import json
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
LIB = ROOT / 'lib'

def read(name):
    return (LIB / name).read_text()

def between(text, start, stop):
    return text[text.index(start):text.index(stop)]

def prepare(out):
    out.mkdir(parents=True, exist_ok=True)
    # Include expansion keeps all musical code sourced directly from lib/.
    overrides = {
        'sampleinfo.h': read('sampleinfo.h').split('#ifdef NOSDCARD')[0]+'\n#endif\n',
        'savefile.h': read('savefile.h').split('#ifdef NOSDCARD')[0]+'\n#endif\n',
        'mcp3208.h': '', 'onewiremidi2.h': '', 'midicallback.h': '',
        'audio_restart.h': '#define AUDIO_RESTART_ENABLED 0\n#define AUDIO_RESTART_WAKE_ENABLED 0\n',
        'WS2812.pio.h': 'static int ws2812_program;\nstatic uint pio_add_program(PIO p,void *v){return 0;}\nstatic void ws2812_program_init(PIO p,uint s,uint o,uint pin,uint freq,uint bits){}\nstatic void pio_sm_put_blocking(PIO p,uint s,uint v){}\n',
    }
    # Keep the virtual LEDs' unscaled colors. The display boundary applies the
    # current brightness, including while the firmware holds a knob indication.
    leds = read('WS2812.h')
    for channel in ('red', 'green', 'blue'):
        statement = f'  {channel} = ({channel} * ws->brightness) / 255;'
        assert leds.count(statement) == 1
        leds = leds.replace(statement, '')
    overrides['WS2812.h'] = leds.replace('  // scale by brightness level',
        '  // Desktop display applies brightness when reading these colors.')
    # These headers are ignored build products in the firmware checkout. Always
    # use the shared generators, so clean checkouts and source archives build
    # the same tables without relying on an earlier hardware build.
    for name, command in {
        'crossfade4_441.h': ['crossfade4.py', '441'],
        'fuzz.h': ['fuzz.py'],
        'resonantfilter_data.h': ['resonantfilter.py'],
    }.items():
        overrides[name] = subprocess.check_output(
            [sys.executable, str(LIB / command[0]), *command[1:]], text=True)
    seen=set()
    def expand(name):
        # Leave system headers and inactive hardware-only includes for Clang.
        # A genuinely missing active dependency must produce an include error.
        if name not in overrides and not (LIB/name).is_file():
            return '#include "'+name+'"\n'
        if name in seen:return ''
        seen.add(name)
        text=overrides[name] if name in overrides else read(name)
        return '\n'+re.sub(r'^[ \t]*#include "([^"]+)".*$',lambda m:expand(m[1]),text,flags=re.M)+'\n'
    pre=['definitions.h','pcg_basic.h','fixedpoint.h','slew.h','utils.h','volume.h',
         'crossfade4_441.h','bitcrush.h','fuzz.h','saturation.h','shaper.h','random.h',
         'array_resample.h','WS2812.h','beatrepeat.h','button_change.h','clock_input.h',
         'comb.h','debounce.h','dust.h','envelope2_fp.h','envelope_linear_integer.h',
         'file_list.h','filterexp.h','freeverb_fp.h','gate.h','knob_change.h',
         'sequencehandler.h','tapedelay.h','taptempo.h','dazzle.h','ectocore_easing.h',
         'noise.h','resonantfilter.h','savefile.h','globals.h']
    source='#include "host.h"\n'+''.join(expand(n) for n in pre)
    source+='\n#include "media.inc"\n'
    source+=expand('realtime_stretch.h')
    source+='\n#include "media_control.inc"\n'
    source+=expand('transfer.h')
    source+=between(read('sdcard_startup.h'),'void update_reverb()', 'static int deferred_preset_load')
    source+=expand('audio_callback.h')
    source+=(ROOT/'main.c').read_text().split('#include "lib/includes.h"',1)[1].split('#ifdef INCLUDE_ZEPTOCORE\n#include "lib/zeptocore.h"')[0]
    ecto=expand('ectocore.h')
    ecto=ecto.replace('  WS2812 *ws2812;\n','')
    # Firmware spins freely while a knob overlay is held (the sleep is only
    # in the position-display branch). Desktop controls tick once per ms,
    # so 10,000 iterations would hide playback position for ten seconds.
    # Keep feedback visible for one second without changing hardware timing.
    overlay_time = 'const uint16_t debounce_ws2812_set_wheel_time = 10000;'
    assert ecto.count(overlay_time) == 1
    ecto=ecto.replace(overlay_time,
                      'const uint16_t debounce_ws2812_set_wheel_time = 1000;')
    # Hardware connection probes have no place in a virtual jack. Preserve the
    # musical unplug handling while using explicit Rack cable presence.
    a=ecto.index('    // Probe every channel')
    b=ecto.index('    if (fuzz_manual_lock',a)
    ecto=ecto[:a]+'''    for(unsigned ch=0;ch<3;++ch) {
      cv_was_unplugged[ch]=cv_plugged[ch]&&!host_controls.connected[ch];
      cv_plugged[ch]=host_controls.connected[ch];
    }
'''+ecto[b:]
    # Reboot is an event for the owner, never an infinite wait on the UI thread.
    ecto=re.sub(r'for \(;;\) \{\s*__wfi\(\);\s*\}', 'return;',ecto)
    fn=ecto.index('void __not_in_flash_func(input_handling)() {')
    loop=ecto.index('  while (1) {',fn)
    prefix=ecto[fn:loop]
    # Persistent locals retain exactly the values the hardware loop retains.
    # Parse declarations with Clang below, instead of guessing C syntax.
    ecto=ecto[:loop]+'''  do {
    if(host_selection_changed){sel_bank_next_new=sel_bank_cur;sel_sample_next_new=sel_sample_cur;debounce_file_change=0;host_selection_changed=false;}
'''+ecto[loop+len('  while (1) {'):]
    end=ecto.rfind('  }\n}')
    assert end>=0
    ecto=ecto[:end]+'  } while (0);\n}'+ecto[end+5:]
    source+=ecto
    source+='\n#include "api.inc"\n'
    (out/'firmware.c').write_text(source)
    return prefix

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--out',type=Path,required=True)
    parser.add_argument('--clang',default='clang')
    parser.add_argument('--source-only',action='store_true')
    parser.add_argument('--target',default=None)
    args=parser.parse_args()
    prepare(args.out)
    if args.source_only:return
    generate(args.out,args.clang,args.target)

def generate(out,clang,target=None):
    flags=['-std=c11','-Wno-everything','-Werror=implicit-function-declaration',
           '-I'+str(LIB/'core_engine'),'-I'+str(LIB)]
    if target:flags.append('--target='+target)
    cpp=subprocess.check_output([clang,*flags,'-E','-P',str(out/'firmware.c')],text=True)
    # Large immutable lookup tables need not be expanded into 100k AST nodes.
    analysis=re.sub(r'(const\s+\w+\s+\w+\[\d+\]\s*=\s*)(\{[^{}]{4000,}\})',
                    lambda m:m[1]+'{0}'+(' '*(len(m[2])-3)),cpp)
    # Clang reports byte offsets; Windows newline translation would make those
    # offsets disagree with the preprocessed string that we rewrite below.
    unit=out/'analysis.c';unit.write_text(analysis, newline='\n')
    with (out/'ast.json').open('w') as f:
        subprocess.run([clang,*flags,'-Xclang','-ast-dump=json','-fsyntax-only',str(unit)],stdout=f,check=True)
    ast=json.loads((out/'ast.json').read_text())
    def span(n):
        r=n['range'];return r['begin']['offset'],r['end']['offset']+r['end']['tokLen']
    def walk(n):
        if not n:return
        yield n
        for c in n.get('inner',[]):yield from walk(c)
    functions=[n for n in ast['inner'] if n['kind']=='FunctionDecl' and any(c['kind']=='CompoundStmt' for c in n.get('inner',[]))]
    controls=next(f for f in functions if f.get('name')=='input_handling')
    cb=next(c for c in controls['inner'] if c['kind']=='CompoundStmt')
    loop=next(c for c in cb['inner'] if c['kind']=='DoStmt')
    loop_start=span(loop)[0]
    input_ids={v['id'] for c in cb['inner'] if c['kind']=='DeclStmt' and span(c)[0]<loop_start for v in c.get('inner',[]) if v['kind']=='VarDecl'}
    variables=[]
    for n in ast['inner']:
        if n['kind']=='VarDecl' and n.get('storageClass')!='extern' and 'range' in n:
            t=n['type']['qualType']
            if not (t.startswith('const ') and '*' not in t):variables.append((n,None))
    for fn in functions:
        for n in walk(fn):
            if n['kind']=='VarDecl' and (n.get('storageClass')=='static' or n['id'] in input_ids):
                if n['id'] not in input_ids and n['type']['qualType'].startswith('const ') and '*' not in n['type']['qualType']:continue
                variables.append((n,fn['name']))
    fields={n['id']:('s_'+(fn+'_' if fn else '')+n['name']) for n,fn in variables}
    # A forward extern and its definition represent the same object.
    globals_by_name={n['name']:fields[n['id']] for n,fn in variables if fn is None}
    for n in ast['inner']:
        if n['kind']=='VarDecl' and n.get('name') in globals_by_name:fields[n['id']]=globals_by_name[n['name']]
    references=[]
    for n in walk(ast):
        if n['kind']=='DeclRefExpr' and n.get('referencedDecl',{}).get('id') in fields:
            a,b=span(n);references.append((a,b,'(engine_current->'+fields[n['referencedDecl']['id']]+')'))
    def rewrite(a,b,extra=()):
        edits=[e for e in references if a<=e[0] and e[1]<=b]+list(extra)
        # Outer edits (declaration removal) take precedence over inner refs.
        edits.sort(key=lambda e:(e[0],-e[1]));result=[];at=a
        for x,y,s in edits:
            if x<at:continue
            result.extend((cpp[at:x],s));at=y
        result.append(cpp[at:b]);return ''.join(result)
    field_decls=[];defaults=[];removals=[];input_assign=[]
    for n,fn in variables:
        a,b=span(n);end=cpp.index(';',b)+1;name=fields[n['id']]
        typ=n['type']['qualType'];typ=re.sub(r'^const ','',typ)
        if '(unnamed ' in typ or '(anonymous ' in typ:
            prefix=cpp[a:n['loc']['offset']]
            typ=re.sub(r'\b(static|extern)\b','',prefix).strip()
        array=typ.find('[')
        decl=(typ[:array]+' '+name+typ[array:]) if array>=0 else typ+' '+name
        field_decls.append(decl+';')
        init=n.get('inner',[])[-1] if n.get('init') else None
        assignment=''
        if init:
            x,y=span(init);expr=rewrite(x,y)
            if array>=0 or expr.lstrip().startswith('{'):
                assignment='memcpy(&engine_current->'+name+', &('+typ+')'+expr+', sizeof(engine_current->'+name+'));'
            else:assignment='engine_current->'+name+' = '+expr+';'
        if n['id'] in input_ids:
            input_assign.append((a,end,assignment))
        else:
            removals.append((a,end,';'))
            if assignment:defaults.append(assignment)
    # Overlapping declarations ("int a,b") are removed as a unit.
    def merge(edits):
        result=[]
        for a,b,s in sorted(edits):
            if result and a<result[-1][1]:
                x,y,t=result.pop();result.append((x,max(y,b),t+'\n'+s))
            else:result.append((a,b,s))
        return result
    removals=merge(removals);input_assign=merge(input_assign)
    body_edits=[];bodies=[]
    for fn in functions:
        body=next(c for c in fn['inner'] if c['kind']=='CompoundStmt');a,b=span(body)
        extra=[r for r in removals+input_assign if a<=r[0] and r[1]<=b]
        if fn['name']=='input_handling':
            extra.extend([(a+1,a+1,'\nif(!engine_current->s_host_input_initialized){\n'),
                          (loop_start,loop_start,'engine_current->s_host_input_initialized=1;\n}\n')])
        start=span(fn)[0]
        bodies.append(cpp[start:a]+rewrite(a,b,extra))
        body_edits.append((a,b,';'))
    # The preamble retains original types, constants, and function prototypes.
    # Suppress ref rewrites here: remaining constant expressions are immutable.
    edits=merge([r for r in removals if not any(x<=r[0]<y for x,y,_ in body_edits)]+body_edits)
    at=0;preamble=[]
    for a,b,s in edits:preamble.extend((cpp[at:a],s));at=b
    preamble.append(cpp[at:])
    result=''.join(preamble)+'\nstruct CoreEngine {\n'+'\n'.join(field_decls)+'\n};\n'
    result+='static _Thread_local CoreEngine *engine_current;\n'
    result+='static CoreEngine *core_enter(CoreEngine *e){CoreEngine *old=engine_current;engine_current=e;return old;}\nstatic void core_leave(CoreEngine *e){engine_current=e;}\n'
    result+='\n'.join(bodies)
    result+='\nstatic void core_defaults(void){\n'+'\n'.join(defaults)+'\n}\n'
    result+='''
CoreEngine *core_engine_create(uint64_t seed) {
    CoreEngine *e=calloc(1,sizeof(CoreEngine));if(!e)return 0;
    CoreEngine *old=core_enter(e);core_defaults();
    if(!host_start_engine(seed)) {
        core_leave(old);core_engine_destroy(e);return 0;
    }
    core_leave(old);return e;
}
void core_engine_destroy(CoreEngine *e) {
    if(!e)return;
    for(unsigned i=0;i<e->s_host_allocation_count;++i)free(e->s_host_allocations[i]);
    free(e);
}
'''
    (out/'engine.c').write_text(result)
    (out/'state-fields.json').write_text(json.dumps({n['name']:fields[n['id']] for n,_ in variables},indent=2))
    print(f'Generated engine with {len(variables)} independent state fields from shared firmware')

if __name__=='__main__':main()
