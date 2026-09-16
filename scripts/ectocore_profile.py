#!/usr/bin/env python3
"""Read fixed, sequence-checked callback profiles through an existing OpenOCD.

Read-only: validates the exact ELF/session and never halts, writes a request,
changes playback or starts a second OpenOCD process.
"""
import argparse
import json
from pathlib import Path
import time

from zeptocore_debug_protocol import ElfImage, OpenOcdRpc, DeviceReader

STAGES = ['open', 'close', 'seek', 'read', 'stretch_seek', 'stretch_read',
          'beat_repeat', 'saturate', 'shaper', 'fuzz', 'bitcrush', 'resample',
          'filter', 'pan', 'reverb', 'delay_setup', 'delay', 'comb',
          'digital', 'conversion_and_submit', 'stretch_inclusive']
WORDS = 13 + len(STAGES)


def decode(data, slot):
    stages = dict(zip(STAGES, data[13:]))
    # Stretch includes its read/seek operations; count those only once.
    measured = sum(stages.values())
    if stages['stretch_inclusive']:
        measured -= stages['stretch_seek'] + stages['stretch_read']
    def source(word):
        return {'bank': (word >> 12) & 15, 'sample': (word >> 8) & 15,
                'variant_id': word & 255, 'mapped': bool(word & (1 << 30)),
                'muted': bool(word & (1 << 31))}
    return {'slot': slot, 'sequence': data[0], 'callback': data[1],
            'start_us': data[2], 'total_us': data[3],
            'source_before': source(data[4]), 'source_after': source(data[5]),
            'effects_before': data[6], 'effects_after': data[7],
            'pitch_index': data[8], 'stretch_q8': data[9],
            'event': data[10], 'event_us': data[11], 'relation': data[12],
            'stages_us': stages, 'unattributed_us': data[3] - measured,
            'host_time': time.time()}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--elf', required=True, type=Path)
    p.add_argument('--server-artifacts', required=True, type=Path)
    p.add_argument('--out', required=True, type=Path)
    p.add_argument('--duration', type=float, default=100)
    args = p.parse_args()
    if args.duration <= 0:
        p.error('duration must be positive')
    elf = ElfImage(args.elf)
    address, size = elf.symbols.get('audio_profile_records', (0, 0))
    if size != WORDS * 4 * 4 or not 0x20000000 <= address < address + size <= 0x20040000:
        raise RuntimeError('ELF has no supported detailed profile records')
    command = json.loads((args.server_artifacts / 'openocd.log').read_text().splitlines()[0])['command']
    port = int(next(v.split()[-1] for v in command if v.startswith('tcl_port ')))
    rpc = OpenOcdRpc(port)
    try:
        reader = DeviceReader(elf, rpc)
        reader.validate()
        previous = [0] * 4
        deadline = time.monotonic() + args.duration
        with args.out.open('x') as out:
            while time.monotonic() < deadline:
                reader.validate()
                for slot in range(4):
                    base = address + slot * WORDS * 4
                    for attempt in range(3):
                        before = rpc.read(base, 1)[0]
                        if before & 1:
                            continue
                        if before == previous[slot]:
                            break
                        data = rpc.read(base, WORDS)
                        after = rpc.read(base, 1)[0]
                        if before == data[0] == after:
                            out.write(json.dumps(decode(data, slot)) + '\n')
                            out.flush()
                            previous[slot] = after
                            break
                time.sleep(1)
    finally:
        rpc.close()


if __name__ == '__main__':
    main()
