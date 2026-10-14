"""Sleep-safe SSH and resumable transfers for the macOS 11 release builder."""
from __future__ import annotations

import shlex
import subprocess
import time

SSH = ['ssh', '-o', 'BatchMode=yes', '-o', 'ForwardAgent=no',
       '-o', 'ConnectTimeout=10', '-o', 'ServerAliveInterval=15',
       '-o', 'ServerAliveCountMax=4']
CAFFEINATE = ['/usr/bin/caffeinate', '-i']
TRANSFER_ATTEMPTS = 3
TRANSIENT_RSYNC_ERRORS = {10, 12, 30, 35, 255}


def ssh_command(host, script):
    # Hold sleep assertions during cloning and packaging too, on both Macs.
    remote = shlex.join([*CAFFEINATE, '/bin/bash', '-c',
                         'set -euo pipefail\n' + script])
    return [*CAFFEINATE, *SSH, host, remote]


def intel_git(command):
    # Big Sur's Xcode Git can stall in its HTTP/2 transport. Use HTTP/1.1
    # just for these commands, and fail a stalled clone/fetch after a minute.
    return [command[0], '-c', 'http.version=HTTP/1.1',
            '-c', 'http.lowSpeedLimit=1000', '-c', 'http.lowSpeedTime=60', *command[1:]]


def rsync(runner, *paths):
    # Inputs/results are mostly compressed archives. Avoid negotiating rsync
    # compression between Apple's openrsync and the macOS 11 rsync 2.6.9.
    command = [*CAFFEINATE, 'rsync', '-c', '--partial', '--no-times',
               '--timeout=120', '--rsync-path=/usr/bin/caffeinate -i /usr/bin/rsync',
               '-e', shlex.join(SSH), *paths]
    for attempt in range(1, TRANSFER_ATTEMPTS + 1):
        try:
            return runner.run(*command)
        except subprocess.CalledProcessError as error:
            if error.returncode not in TRANSIENT_RSYNC_ERRORS or attempt == TRANSFER_ATTEMPTS:
                raise
            runner.write(f'Transfer interrupted (exit {error.returncode}); '
                         f'retrying with partial files ({attempt + 1}/{TRANSFER_ATTEMPTS})')
            time.sleep(2)
