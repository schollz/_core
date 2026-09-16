#!/usr/bin/env python3
"""Read fixed, sequence-checked clock latency records through an existing OpenOCD.

Read-only: validates the exact ELF/session and never halts, writes a request,
changes playback or starts a second OpenOCD process.
"""
import argparse
import json
from pathlib import Path
import time

from zeptocore_debug_protocol import ElfImage, OpenOcdRpc, DeviceReader

FIELDS = ['sequence', 'id', 'input_us', 'handler_done_us', 'phase_us', 'phase',
          'beat', 'applied_us', 'submit_us', 'dma_us', 'dma_offset',
          'first_nonzero', 'flags', 'negative_latency', 'source', 'mode',
          'starvation_before', 'starvation_after', 'source_frames', 'bytes_per_frame']
WORDS = len(FIELDS)


def decode(data, slot, rate):
    result = dict(zip(FIELDS, data))
    result['slot'] = slot
    result['sample_rate_millihz'] = rate
    result['host_time'] = time.time()
    for key in ['handler_done_us', 'phase_us', 'applied_us', 'submit_us', 'dma_us']:
        result[key + '_from_input'] = ((result[key] - result['input_us']) & 0xffffffff) if result[key] else None
    if result['dma_us']:
        result['beat_output_us'] = result['dma_us_from_input'] + result['dma_offset'] * 1e9 / rate
        # Only claim onset within this consumer block. A split/late onset needs
        # a separate tagged-buffer trace; do not extrapolate across DMA gaps.
        if result['first_nonzero'] != 0xffffffff and result['dma_offset'] + result['first_nonzero'] < 256:
            result['pcm_onset_us'] = result['beat_output_us'] + result['first_nonzero'] * 1e9 / rate
    return result


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
    address, size = elf.symbols.get('clock_latency_records', (0, 0))
    if size != WORDS * 4 * 8 or not 0x20000000 <= address < address + size <= 0x20040000:
        raise RuntimeError('ELF has no supported detailed profile records')
    command = json.loads((args.server_artifacts / 'openocd.log').read_text().splitlines()[0])['command']
    port = int(next(v.split()[-1] for v in command if v.startswith('tcl_port ')))
    rpc = OpenOcdRpc(port)
    try:
        reader = DeviceReader(elf, rpc)
        header = reader.validate()
        rate = header[22]
        previous = [0] * 8
        deadline = time.monotonic() + args.duration
        with args.out.open('x') as out:
            while time.monotonic() < deadline:
                reader.validate()
                for slot in range(8):
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
                            out.write(json.dumps(decode(data, slot, rate)) + '\n')
                            out.flush()
                            previous[slot] = after
                            break
                time.sleep(1)
    finally:
        rpc.close()


if __name__ == '__main__':
    main()
