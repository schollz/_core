#!/usr/bin/env python3
"""Run an actual ENOSPC/retry check on a disposable, bounded HFS+ image."""
import argparse
import pathlib
import subprocess
import tempfile
import sys


def run(*args):
    subprocess.run(args, check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable', type=pathlib.Path)
    args = parser.parse_args()
    if sys.platform != 'darwin':
        parser.error('This acceptance harness requires macOS hdiutil')
    executable = args.executable.resolve(strict=True)
    native_temp = pathlib.Path(tempfile.gettempdir()) / executable.name
    native_temp.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='core-disk-full-test-', dir=native_temp) as directory:
        root = pathlib.Path(directory)
        image = root / 'disposable.dmg'
        mount = root / 'mount'
        mount.mkdir()
        run('hdiutil', 'create', '-size', '16m', '-fs', 'HFS+', '-volname',
            'Core ENOSPC Test', str(image))
        run('hdiutil', 'attach', '-nobrowse', '-mountpoint', str(mount), str(image))
        try:
            (mount / 'disposable-test-volume').write_text('Only synthetic acceptance data.\n')
            run(str(executable), '--disk-full-test', str(mount))
        finally:
            run('hdiutil', 'detach', str(mount))


if __name__ == '__main__':
    main()
