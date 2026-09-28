#!/usr/bin/env python3
"""Exercise the shared bank request path on an Ectocore test-control build.

Sends only reserved serial selection controls (116 bank, 117 valid-slot ordinal).
Reads live RAM and the diagnostic mailbox through SWD; never halts the target,
changes sample files, saves presets, or writes playback variables through SWD.
Requires pyserial, SEEK_DIAGNOSTICS and SEEK_TEST_CONTROLS.
"""
import argparse
import json
from pathlib import Path
import time
import serial
from zeptocore_debug_protocol import OpenOcdRpc, ElfImage, DeviceReader


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--elf', type=Path, required=True)
    parser.add_argument('--rpc-port', type=int, default=6666)
    parser.add_argument('--port', required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--cycles', type=int, default=2)
    args = parser.parse_args()
    if args.cycles < 1:
        parser.error('--cycles must be positive')
    args.out.mkdir(parents=True, exist_ok=True)
    elf = ElfImage(args.elf)
    rpc = OpenOcdRpc(args.rpc_port)
    reader = DeviceReader(elf, rpc)
    header = reader.validate()
    deadline = time.monotonic() + 30
    while header[21] != 3 and time.monotonic() < deadline:
        time.sleep(.1)
        header = reader.validate()
    if not header[4] & 4 or header[21] != 3:
        raise RuntimeError('requires a running test-control build after startup')

    def read(name):
        address, length = elf.symbols[name]
        return bytes(int(x, 0) for x in rpc.command(f'read_memory {address} 8 {length}').split())

    # RP2040 catalogue layouts follow bank_metadata.h/sampleinfo.h and the
    # retained GDB layout.txt. SampleList=12, Sample=8, SampleInfo.filename_index=44.
    banks = []
    for bank, address in enumerate(rpc.read(elf.symbols['banks'][0], 16)):
        words = rpc.read(address, 3)
        count = words[0] & 65535
        if not count:
            continue
        assert count <= 16
        entries = []
        for ordinal in range(count):
            info = rpc.read(words[1] + ordinal * 8, 1)[0]
            slot = rpc.read(info + 44, 1)[0] & 255
            entries.append({'ordinal': ordinal, 'filename': slot, 'info': info})
        banks.append({'bank': bank, 'samples': entries, 'bytes': words[2]})
    (args.out / 'catalogue.json').write_text(json.dumps(banks, indent=2) + '\n')
    before = reader.snapshot()
    records = []
    with serial.Serial(args.port, 115200, timeout=0, write_timeout=1) as port, \
            (args.out / 'selections.jsonl').open('w') as log:
        time.sleep(.3)  # Allow USB CDC line state to settle after opening.
        for cycle in range(args.cycles):
            for bank in banks:
                b = bank['bank']
                for entry in bank['samples']:
                    s = entry['ordinal']
                    port.write(bytes((ord('T'), 116, b, ord('T'), 117, s)))
                    deadline = time.monotonic() + 4
                    time.sleep(.025)
                    while True:
                        status = reader.metadata_status()
                        actual = read('sel_bank_cur')[0], read('sel_sample_cur')[0]
                        pending = read('fil_current_change')[0]
                        if (actual == (b, s) and status['state'] == 0 and
                                not pending and read('fil_is_open')[0]):
                            break
                        if time.monotonic() > deadline:
                            raise RuntimeError((b, s, actual, status, pending))
                        time.sleep(.005)
                    name = read('fil_current_name').split(b'\0')[0].decode()
                    assert name.startswith(f"bank{b+1}/{entry['filename']}.")
                    assert status['resident_bank'] == b
                    assert not status['last_error'] and not status['rollbacks']
                    for other in banks:
                        ptr = rpc.read(other['samples'][0]['info'] + 12, 1)[0]
                        assert bool(ptr) == (other['bank'] == b)
                    record = {'cycle': cycle, 'bank': b, 'sample': s, 'file': name, **status}
                    records.append(record)
                    log.write(json.dumps(record) + '\n')
                    log.flush()
                    if s == 0:
                        print(cycle, b, status, flush=True)
    after = reader.snapshot()
    (args.out / 'navigation.json').write_text(json.dumps({
        'elf_sha256': elf.sha256, 'before': before, 'after': after,
        'selections': records}, indent=2) + '\n')
    print(f'Verified {len(records)} selections', flush=True)
    rpc.close()


if __name__ == '__main__':
    main()
