#!/usr/bin/env python3
"""Reject stale dependency checkouts without overwriting local changes."""
from pathlib import Path
import subprocess
import sys


def check(path, tag):
    def git(*args):
        return subprocess.check_output(['git', '-C', str(path), *args], text=True,
                                       stderr=subprocess.PIPE).strip()
    try:
        if git('rev-parse', 'HEAD') != git('rev-parse', f'refs/tags/{tag}^{{commit}}'):
            raise ValueError(f'checkout does not match {tag}')
        status = git('submodule', 'status', '--recursive')
        # Preserve the leading status character (strip above removes the first
        # clean space, but never a mismatch marker).
        if any(line.startswith(('-', '+', 'U')) for line in status.splitlines()):
            raise ValueError('submodules are missing or at different revisions')
    except (subprocess.CalledProcessError, ValueError) as exc:
        raise SystemExit(
            f'{path}: expected {tag} with initialized matching submodules ({exc}).\n'
            'Preserve this directory by moving it aside, then rerun make to clone '
            'the pinned version. Use a fresh build directory after upgrading. '
            'Local changes have not been modified.') from exc


if __name__ == '__main__':
    if len(sys.argv) != 5:
        raise SystemExit('usage: check_pico_dependencies.py SDK_PATH SDK_TAG EXTRAS_PATH EXTRAS_TAG')
    check(Path(sys.argv[1]), sys.argv[2])
    check(Path(sys.argv[3]), sys.argv[4])
