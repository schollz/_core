"""Exercise the real Make recipes without building firmware or flashing hardware."""
from pathlib import Path
import json
import os
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
DEVICES = ('zeptocore', 'ezeptocore', 'ectocore')


class VisualizerMakeTests(unittest.TestCase):
    def test_build_then_upload(self):
        for device in DEVICES:
            for fail_build in (False, True):
                with self.subTest(device=device, fail_build=fail_build), tempfile.TemporaryDirectory() as directory:
                    path = Path(directory)
                    log = path / 'calls.jsonl'
                    fake_make = path / 'make'
                    fake_make.write_text('''#!/usr/bin/env python3
import json, os, sys
with open(os.environ['MAKE_CALLS'], 'a') as log:
    log.write(json.dumps(sys.argv[1:]) + '\\n')
if os.environ['FAIL_BUILD'] == '1' and sys.argv[1] != 'upload-built':
    sys.exit(1)
''')
                    fake_make.chmod(0o755)
                    # Same-name files must never suppress these phony targets.
                    (path / f'{device}-visualizer').touch()
                    result = subprocess.run([
                        'make', '-f', str(ROOT / 'Makefile'), '-j4', f'{device}-visualizer',
                        f'MAKE={fake_make}', 'ZEPTOCORE_VISUALIZER=OFF',
                        'UPLOAD_UF2=wrong.uf2',
                    ], cwd=path, env={**os.environ, 'MAKE_CALLS': str(log), 'FAIL_BUILD': str(int(fail_build))},
                       text=True, capture_output=True)
                    calls = [json.loads(line) for line in log.read_text().splitlines()]
                    expected = [[device, 'ZEPTOCORE_VISUALIZER=ON']]
                    if not fail_build:
                        expected.append(['upload-built', f'UPLOAD_UF2={device}_visualizer.uf2'])
                    self.assertEqual(calls, expected, result.stdout + result.stderr)
                    self.assertEqual(result.returncode == 0, not fail_build)

    def test_default_device_and_artifact_selection(self):
        dependencies = ['pico-sdk', 'pico-extras', 'check-pico-dependencies',
                        'lib/fuzz.h', 'lib/transfer_saturate2.h', 'lib/sinewaves2.h',
                        'lib/crossfade4_441.h', 'lib/resonantfilter_data.h', 'lib/cuedsounds.h']
        for device in DEVICES:
            for mode in (None, 'ON', 'OFF'):
                with self.subTest(device=device, mode=mode):
                    command = ['make', '-n', '-j4', device, 'MAKE=echo']
                    for dependency in dependencies:
                        command += ['-o', dependency]
                    if mode:
                        command += [f'ZEPTOCORE_VISUALIZER={mode}']
                    result = subprocess.run(command, cwd=ROOT, text=True, capture_output=True, check=True)
                    recipe = result.stdout
                    self.assertIn(f'-DZEPTOCORE_VISUALIZER={mode or "OFF"}', recipe)
                    self.assertIn(f'/lib/cmake/{device}_compile_definitions.cmake', recipe)
                    suffix = '_visualizer' if mode == 'ON' else ''
                    self.assertIn(f'cp build/_core.uf2 {device}{suffix}.uf2', recipe)
                    self.assertNotIn('picotool load', recipe)


if __name__ == '__main__':
    unittest.main()
