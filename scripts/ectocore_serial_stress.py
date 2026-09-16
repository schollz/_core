#!/usr/bin/env python3
"""Exercise musical controls through ectocore's test-only serial transport.

Requires SEEK_TEST_CONTROLS firmware and pyserial. Does not save presets.
Run alongside a diagnostic and analog audio capture. Sent commands alone are
not evidence of coverage: inspect the captured playback states and counters.
"""
import argparse
import json
from pathlib import Path
import time

import serial

from zeptocore_debug import request
from zeptocore_debug_server import DEFAULT_SOCKET


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--socket', default=DEFAULT_SOCKET)
    parser.add_argument('--phase-seconds', type=float, default=20)
    args = parser.parse_args()
    info = request(args.socket, 'device.info')['data']
    if not info['features'] & 4 or info['stage'] != 3:
        raise RuntimeError('requires active test-control firmware')
    if args.phase_seconds <= 0:
        parser.error('phase-seconds must be positive')
    with serial.Serial(args.port, 115200, timeout=0, write_timeout=1) as port, \
            args.out.open('x') as log:
        def cc(controller, value):
            port.write(bytes((ord('T'), controller, value)))
            log.write(json.dumps({'time': time.time(), 'cc': controller,
                                  'value': value}) + '\n')

        # Establish a known source rate and disable optional random controls.
        def clear():
            for c, v in [(15, 85), (16, 64), (18, 0), (20, 0),
                         (22, 0), (25, 0), (110, 0), (112, 0)]:
                cc(c, v)

        try:
            clear()
            for phase in ['switching', 'effects', 'combined', 'stretch-reverse']:
                print(phase, flush=True)
                log.write(json.dumps({'time': time.time(), 'phase': phase})+'\n')
                deadline = time.monotonic() + args.phase_seconds
                index = 0
                while time.monotonic() < deadline:
                    if phase != 'effects':
                        cc(21, (index % 8) * 16)  # four selections per second
                    if phase != 'switching':
                        cc(25, 127)  # maximum break/effect probability
                        cc(26, ((index // 4) % 7) * 18)  # cycle effect banks
                        cc(18, 100)  # amen/retrigger sequence
                    if phase == 'stretch-reverse':
                        cc(20, 96)
                        cc(110, 127)
                    index += 1
                    log.flush()
                    time.sleep(.25)
        finally:
            clear()
            cc(21, 0)
            log.flush()


if __name__ == '__main__':
    main()
