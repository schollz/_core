#!/usr/bin/env python3
"""Exercise protected empty-slot preset save/load through normal MIDI test controls.

Requires already-programmed test firmware with feature bit 512. Does not flash,
halt, or send filesystem commands through diagnostics. Leaves the new preset in
the selected previously empty slot, default 15; existing slots are protected by
firmware. Records Scarlett audio across the acknowledged save/load operations.
"""
import argparse
import json
from pathlib import Path
import subprocess
import sys
import time

from zeptocore_debug import request
from zeptocore_debug_protocol import ElfImage
from zeptocore_midi import wait_available


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--elf',required=True,type=Path)
    p.add_argument('--out',required=True,type=Path)
    p.add_argument('--slot',type=int,default=15)
    p.add_argument('--midi-port',default='hw:6,0,0')
    p.add_argument('--audio-device',default='hw:4,0')
    args=p.parse_args()
    if not 0<=args.slot<16:p.error('slot must be 0..15')
    args.out.mkdir(parents=True,exist_ok=False)
    elf=ElfImage(args.elf);(args.out/'firmware.elf').write_bytes(elf.data)
    root=Path(__file__).resolve().parents[1]
    socket=str((args.out/'debug.sock').resolve());events=[];snapshots=[]

    def midi(packet):
        events.append({'time':time.time(),'midi_hex':packet})
        subprocess.run(['amidi','-p',args.midi_port,'-S',packet],check=True)

    def snapshot(label):
        data=request(socket,'seek.status')['data'];snapshots.append(data)
        (args.out/(label+'.json')).write_text(json.dumps(data,indent=2)+'\n')
        return data

    def identity(s):
        p=s['audio']['playback']
        return [p[k] for k in ['bank','sample','variation','audio_variant']]

    with (args.out/'server.log').open('w') as log,(args.out/'audio.log').open('w') as audio_log:
        server=subprocess.Popen([sys.executable,str(root/'scripts/zeptocore_debug_server.py'),
            '--elf',str(args.elf),'--socket',socket,'--artifacts',str(args.out/'server')],
            stdout=log,stderr=subprocess.STDOUT)
        audio=None;capture=False
        try:
            deadline=time.monotonic()+10
            while not Path(socket).exists():
                if server.poll() is not None or time.monotonic()>deadline:raise RuntimeError('server failed')
                time.sleep(.1)
            if not request(socket,'device.info')['data']['features']&512:
                raise RuntimeError('firmware lacks protected preset controls')
            deadline=time.monotonic()+30
            while True:
                ready=request(socket,'seek.status')['data']
                playback=ready['audio']['playback']
                if ready['device']['stage']==3 and playback['file_open'] and not playback['media_withheld']:break
                if time.monotonic()>deadline:raise RuntimeError('playback did not leave its boot guard')
                time.sleep(.1)
            cached=request(socket,'media.cached')['data']
            (args.out/'cached-before.json').write_text(json.dumps(cached,indent=2)+'\n')
            if not cached['available'] or cached['values']['savefile_has_data'][args.slot]:
                raise RuntimeError('requested preset slot is not verified empty')
            wait_available(args.midi_port)
            midi('B01200B01600B01800B01900B01040B01500B01400B06E00B07000B07100FA')
            time.sleep(.6)
            first=snapshot('before-save')
            request(socket,'capture.start',metadata={'workload':'protected preset save/load',
                'slot':args.slot,'audio_file':str(args.out/'output.wav'),'polling':'event observations'})
            capture=True
            audio=subprocess.Popen(['arecord','-D',args.audio_device,'-f','S32_LE','-r','48000',
                '-c','2','-d','15','-t','wav',str(args.out/'output.wav')],stdout=audio_log,stderr=subprocess.STDOUT)
            midi(f'B072{args.slot:02X}');time.sleep(.6)
            saved=snapshot('after-save')
            if saved['audio']['counters']['file_generation']==first['audio']['counters']['file_generation']:
                raise RuntimeError('save did not reopen playback; slot may already contain a preset')
            assert saved['control']['maps']['ownership_acquisitions']>first['control']['maps']['ownership_acquisitions']
            assert request(socket,'media.cached')['data']['values']['savefile_has_data'][args.slot]
            assert identity(saved)==identity(first) and saved['audio']['playback']['file_open']
            midi('B01540FA');time.sleep(.6)
            different=snapshot('different-selection')
            assert identity(different)!=identity(first)
            midi(f'B073{args.slot:02X}');time.sleep(.8)
            loaded=snapshot('after-load')
            assert identity(loaded)==identity(first) and loaded['audio']['playback']['file_open']
            # A cache miss remains playable; an explicit stop supplies the
            # existing quiet window for loading its persisted map.
            midi('FC');deadline=time.monotonic()+5
            while request(socket,'maps.status')['data']['pending_job']:
                if time.monotonic()>deadline:raise RuntimeError('map load did not settle')
                time.sleep(.1)
            midi('FA');time.sleep(.5)
            last=snapshot('resumed')
            assert last['audio']['playback']['mapped']
            before,after=first['control']['maps'],last['control']['maps']
            assert before['builds']==after['builds'] and before['index_writes']==after['index_writes']
            assert before['ownership_timeouts']==after['ownership_timeouts']
            assert last['irq']['counters']['starvation_count']==first['irq']['counters']['starvation_count']
            result={'valid':True,'created_preset_slot':args.slot,'elf_sha256':elf.sha256,
                'saved_selection':identity(first),'intermediate_selection':identity(different),
                'loaded_selection':identity(loaded),'mapped_immediately_after_load':loaded['audio']['playback']['mapped'],
                'mapped_after_explicit_quiet_window':last['audio']['playback']['mapped'],
                'map_builds':after['builds']-before['builds'],'index_writes':after['index_writes']-before['index_writes']}
            (args.out/'summary.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result),flush=True)
            if audio.wait(timeout=20):raise RuntimeError('audio recorder failed')
        finally:
            if capture:
                try:request(socket,'capture.stop')
                except (RuntimeError,OSError):pass
            if audio and audio.poll() is None:audio.terminate();audio.wait(timeout=5)
            if events:
                subprocess.run(['amidi','-p',args.midi_port,'-S','B01500B01400B06E00B07000B07100FA'],
                               check=False,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
            server.terminate();server.wait(timeout=10)
            (args.out/'controls.json').write_text(json.dumps(events,indent=2)+'\n')
            (args.out/'snapshots.jsonl').write_text(''.join(json.dumps(s)+'\n' for s in snapshots))


if __name__=='__main__':main()
