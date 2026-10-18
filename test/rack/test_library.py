"""Offline checks of the repository-root contract used by VCV's build tools."""
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('rack_manifest', ROOT / 'rack/scripts/manifest.py')
manifest = importlib.util.module_from_spec(spec)
spec.loader.exec_module(manifest)


class LibraryTests(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name).resolve()
        for name in ('Makefile', 'rack/library.mk', 'rack/Makefile',
                     'plugin.json', 'rack/plugin.json', 'rack/scripts/manifest.py'):
            destination = self.root / name
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / name, destination)
        self.sdk = self.root / 'sdk'
        self.sdk.mkdir()
        # Stand in for the SDK, retaining the real root and Rack Makefiles.
        # This tests forwarding and isolation without requiring a compiler.
        (self.sdk / 'plugin.mk').write_text('''
.DEFAULT_GOAL := all
TARGET := plugin.fixture
OBJECTS :=
.PHONY: all dep cleandep dist cleandist install clean
all: $(TARGET)
$(TARGET):
\t$(PYTHON) record.py all "$(RACK_DIR)" "$(CC)" "$(CXX)" "$(MAKEFLAGS)"
dep cleandep install clean cleandist:
\t$(PYTHON) record.py $@ "$(RACK_DIR)" "$(CC)" "$(CXX)" "$(MAKEFLAGS)"
dist: all
\t$(PYTHON) record.py dist "$(RACK_DIR)" "$(CC)" "$(CXX)" "$(MAKEFLAGS)"
''')
        (self.root / 'rack/record.py').write_text('''
import json, os, shutil, sys
from pathlib import Path
goal = sys.argv[1]
with open('../calls.jsonl', 'a') as log:
    log.write(json.dumps(sys.argv[1:]) + '\\n')
if os.environ.get('FAIL_GOAL') == goal:
    raise SystemExit(7)
if goal in ('clean', 'cleandist'):
    shutil.rmtree('dist', ignore_errors=True)
if goal == 'clean':
    shutil.rmtree('build', ignore_errors=True)
    Path('plugin.fixture').unlink(missing_ok=True)
if goal == 'all':
    Path('plugin.fixture').write_text('fixture binary')
if goal == 'dist':
    Path('dist').mkdir(exist_ok=True)
    Path('dist/InfiniteDigits-2.0.2-fixture.vcvplugin').write_text('fixture package')
''')
        self.env = dict(os.environ)
        for key in ('RACK_DIR', 'MAKEFLAGS', 'MFLAGS', 'MAKEOVERRIDES', 'CC', 'CXX',
                    'PYTHON', 'FAIL_GOAL'):
            self.env.pop(key, None)

    def make(self, *args, env=None, cwd=None, success=True):
        python = 'python' if os.name == 'nt' else sys.executable
        result = subprocess.run(['make', f'PYTHON={python}', *args],
                                cwd=cwd or self.root, env={**self.env, **(env or {})},
                                text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        if success:
            self.assertEqual(result.returncode, 0, result.stdout)
        else:
            self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertNotIn('jobserver unavailable', result.stdout)
        return result.stdout

    def calls(self):
        path = self.root / 'calls.jsonl'
        return [json.loads(line) for line in path.read_text().splitlines()] if path.exists() else []

    def sentinel(self, name):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text('keep')
        return path

    def test_manifests_and_sync(self):
        self.assertEqual(manifest.check(self.root)['slug'], 'InfiniteDigits')
        mirror = self.root / 'plugin.json'
        for content in (None, b'{broken', b'[]', b'{"slug":"different"}'):
            with self.subTest(content=content):
                if content is None:
                    mirror.unlink()
                else:
                    mirror.write_bytes(content)
                with self.assertRaises((ValueError, OSError)):
                    manifest.check(self.root)
                manifest.sync(self.root)
                manifest.check(self.root)

    def test_invalid_authoritative_manifest_cannot_be_synced(self):
        before = (self.root / 'plugin.json').read_bytes()
        (self.root / 'rack/plugin.json').write_text('{broken')
        with self.assertRaises(ValueError):
            manifest.sync(self.root)
        self.assertEqual((self.root / 'plugin.json').read_bytes(), before)

    def test_root_manifest_is_regular_json_in_git(self):
        subprocess.run(['git', 'init', '-q', str(self.root)], check=True)
        subprocess.run(['git', 'add', 'plugin.json'], cwd=self.root, check=True)
        entry = subprocess.check_output(['git', 'ls-files', '-s', 'plugin.json'],
                                        cwd=self.root, text=True)
        self.assertTrue(entry.startswith('100644 '), entry)
        data = subprocess.check_output(['git', 'show', ':plugin.json'], cwd=self.root)
        self.assertEqual(json.loads(data), manifest.check(self.root))

    def test_root_and_direct_builds_reject_drift(self):
        (self.root / 'plugin.json').write_text('{}')
        for cwd, sdk in ((self.root, 'sdk'), (self.root / 'rack', '../sdk')):
            output = self.make('all', f'RACK_DIR={sdk}', cwd=cwd, success=False)
            self.assertIn('differs from rack/plugin.json', output)
        self.assertEqual(self.calls(), [])

    def test_library_sequence_and_cleanup_isolation(self):
        firmware = [self.sentinel(name) for name in
                    ('build/firmware', 'sample-manager/build/app', 'lib/fuzz.h', 'device.uf2')]
        self.sentinel('dist/old.vcvplugin')
        for goal in ('clean', 'cleandep', 'dep', 'dist'):
            self.make(goal, 'RACK_DIR=sdk', '-j2', 'CC=chosen-cc', 'CXX=chosen-cxx')
        self.assertEqual([call[0] for call in self.calls()],
                         ['clean', 'cleandep', 'dep', 'all', 'dist'])
        for call in self.calls():
            self.assertEqual(Path(call[1]).resolve(), self.sdk)
            self.assertEqual(call[2:4], ['chosen-cc', 'chosen-cxx'])
            self.assertIn('j', call[4])
        root_packages = list((self.root / 'dist').glob('*.vcvplugin'))
        self.assertEqual(len(root_packages), 1)
        self.assertEqual(root_packages[0].read_bytes(),
                         (self.root / 'rack/dist' / root_packages[0].name).read_bytes())
        self.assertFalse((self.root / 'dist/old.vcvplugin').exists())
        self.make('clean', env={'RACK_DIR': 'sdk'})
        self.assertFalse((self.root / 'dist').exists())
        self.assertFalse((self.root / 'rack/dist').exists())
        self.assertTrue(all(path.exists() for path in firmware))

    def test_failed_dist_removes_stale_root_package(self):
        self.sentinel('dist/old.vcvplugin')
        self.make('dist', 'RACK_DIR=sdk', env={'FAIL_GOAL': 'dist'}, success=False)
        self.assertFalse((self.root / 'dist').exists())

    def test_environment_sdk_default_and_command_line_precedence(self):
        self.make(env={'RACK_DIR': 'sdk'})
        self.assertEqual(self.calls()[-1][0], 'all')
        self.assertEqual(Path(self.calls()[-1][1]).resolve(), self.sdk)
        self.make('dep', 'RACK_DIR=sdk', env={'RACK_DIR': 'missing'})
        self.assertEqual(self.calls()[-1][0], 'dep')
        self.assertEqual(Path(self.calls()[-1][1]).resolve(), self.sdk)

    def test_default_sdk_discovery_and_root_install(self):
        sdk = self.root / 'artifacts/rack-sdk/Rack-SDK'
        sdk.parent.mkdir(parents=True)
        shutil.copytree(self.sdk, sdk)
        self.make('dist')
        self.make('install')
        self.make('install-vcv')
        self.make('cleandist')
        self.assertEqual([call[0] for call in self.calls()],
                         ['all', 'dist', 'install', 'install', 'cleandist'])
        self.assertFalse((self.root / 'dist').exists())

    def test_clean_with_rack_goal_without_explicit_sdk(self):
        sdk = self.root / 'artifacts/rack-sdk/Rack-SDK'
        sdk.parent.mkdir(parents=True)
        shutil.copytree(self.sdk, sdk)
        firmware = self.sentinel('build/firmware')
        self.make('clean', 'dep')
        self.assertEqual([call[0] for call in self.calls()], ['clean', 'dep'])
        self.assertTrue(firmware.exists())

    def test_firmware_defaults_and_plain_clean(self):
        # Suppress real firmware recipes while checking default target selection.
        (self.root / 'firmware-probe.mk').write_text(
            '.PHONY: dobuild\ndobuild:\n\t@echo firmware-default\n')
        probe = ['-n', '-f', 'Makefile', '-f', 'firmware-probe.mk',
                 '-o', 'pico-sdk', '-o', 'pico-extras', '-o', 'lib/fuzz.h',
                 '-o', 'lib/transfer_saturate2.h', '-o', 'lib/sinewaves2.h',
                 '-o', 'lib/crossfade4_441.h', '-o', 'lib/resonantfilter_data.h',
                 '-o', 'lib/cuedsounds.h', '-o', 'build']
        output = self.make(*probe)
        self.assertIn('firmware-default', output)
        output = self.make(*probe, 'dobuild', 'RACK_DIR=sdk')
        self.assertIn('firmware-default', output)
        firmware = self.sentinel('build/firmware')
        rack = self.sentinel('rack/build/plugin')
        self.make('clean')
        self.assertFalse(firmware.exists())
        self.assertTrue(rack.exists())
        self.assertEqual(self.calls(), [])


if __name__ == '__main__':
    unittest.main()
