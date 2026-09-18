"""Host-independent checks; no SSH, signing, or publication is performed."""
import json
from pathlib import Path
import plistlib
import subprocess
import tempfile
import unittest
from unittest.mock import patch

import release_common as common
import remote_intel as intel


class IntelReleaseTests(unittest.TestCase):
    def test_entry_point_exposes_named_remote_option(self):
        result = subprocess.run(
            ['python3', str(Path(__file__).parents[2] / 'scripts/release_visualizer_macos_intel.py'),
             '--help'], text=True, capture_output=True, check=True)
        self.assertIn('--remote REMOTE_OPTION', result.stdout)
        self.assertIn('remote_host', result.stdout)

    def test_remote_script_is_valid_and_build_only(self):
        script = intel.build_script('/tmp/zeptocore-release-intel.ABC123', '8.0.1', 3)
        subprocess.run(['bash', '-n'], input=script, text=True, check=True)
        for expected in ('x86_64', 'Darwin', 'VISUALIZER_STANDALONE_ONLY=ON',
                         'CMAKE_OSX_DEPLOYMENT_TARGET=11.6', 'zeptocore_visualizer_Standalone'):
            self.assertIn(expected, script)
        for forbidden in ('notarytool', 'APPLE_PASSWORD', 'gh release', '--sign'):
            self.assertNotIn(forbidden, script)
        calls = []
        intel.ssh(lambda args: calls.append(args), 'user@host', script)
        self.assertIn('ForwardAgent=no', calls[0])
        self.assertIn('set -euo pipefail', calls[0][-1])

    def test_bundle_rejects_wrong_architecture_target_and_dependencies(self):
        with tempfile.TemporaryDirectory() as temp:
            app = Path(temp) / 'test.app'
            (app / 'Contents').mkdir(parents=True)
            with (app / 'Contents/Info.plist').open('wb') as stream:
                plistlib.dump({'CFBundleShortVersionString': '8.0.1',
                              'CFBundleExecutable': 'visualizer'}, stream)
            def run(args):
                return {'lipo': arch, '-l': loads, '-L': deps}.get(args[0],
                       {'-l': loads, '-L': deps}.get(args[1]))
            arch, loads, deps = 'x86_64', 'cmd LC_BUILD_VERSION\n minos 11.6\n', 'binary:\n /usr/lib/libc.dylib (version)'
            intel.check_bundle(run, app, '8.0.1')
            arch = 'arm64'
            with self.assertRaises(RuntimeError):
                intel.check_bundle(run, app, '8.0.1')
            arch, loads = 'x86_64', 'cmd LC_BUILD_VERSION\n minos 12.0\n version 11.6\n'
            with self.assertRaises(RuntimeError):
                intel.check_bundle(run, app, '8.0.1')
            loads, deps = 'cmd LC_BUILD_VERSION\n minos 11.6\n', 'binary:\n /usr/local/lib/custom.dylib (version)'
            with self.assertRaises(RuntimeError):
                intel.check_bundle(run, app, '8.0.1')

    def test_pipeline_returns_before_signing_and_stops_on_build_failure(self):
        for fail in (False, True):
            with self.subTest(fail=fail), tempfile.TemporaryDirectory() as temp:
                root = Path(temp)
                events = []
                def snapshot(source):
                    source.mkdir()
                    for name in ('LICENSE', 'Resources/Fonts/IBM-Plex-LICENSE.txt',
                                 'Resources/Fonts/Font-Awesome-LICENSE.txt'):
                        p = source / name
                        p.parent.mkdir(parents=True, exist_ok=True)
                        p.write_text('license')
                    return {}
                def build(run, remote, directory, source, output, version, jobs):
                    events.append('remote-build')
                    if fail:
                        raise RuntimeError('remote build failed')
                    built = output / 'returned'
                    (built / 'zeptocore visualizer.app').mkdir(parents=True)
                    (output / 'JUCE-LICENSE.md').write_text('license')
                    events.append('returned')
                    return built
                def run(args):
                    events.append(str(args[0]))
                    if args[0] == 'security':
                        return common.IDENTITY
                    if args[0] == 'ditto' and '-c' in args:
                        Path(args[-1]).write_bytes(b'zip')
                    return ''
                def ssh(run, remote, script):
                    events.append('cleanup' if script.startswith('rm ') else 'mktemp')
                    return '/tmp/zeptocore-release-intel.ABC123'
                release = {'id': 1, 'tag_name': 'v8.0.1'}
                with patch.object(common, 'APP_ROOT', root), \
                     patch.object(common.platform, 'system', return_value='Darwin'), \
                     patch.object(common.platform, 'machine', return_value='arm64'), \
                     patch.object(common.shutil, 'which', return_value='/tool'), \
                     patch.object(common, 'run', side_effect=run), \
                     patch.object(common, 'snapshot', side_effect=snapshot), \
                     patch.object(common, 'release_info', return_value=release), \
                     patch.object(common, 'publish', side_effect=lambda *a: events.append('publish')), \
                     patch.object(common, 'notarize', side_effect=lambda *a: events.append('notarize')), \
                     patch.object(intel, 'ssh', side_effect=ssh), \
                     patch.object(intel, 'build', side_effect=build), \
                     patch.object(intel, 'check_bundle', side_effect=lambda *a: events.append('verify')), \
                     patch('sys.argv', ['release', '--version', '8.0.1', '--notary-profile', 'profile']):
                    result = common.main('Darwin', 'x86_64', '11.6', remote_intel=True)
                self.assertEqual(result, int(fail))
                if fail:
                    self.assertNotIn('codesign', events)
                    self.assertNotIn('publish', events)
                    self.assertNotIn('cleanup', events)
                else:
                    ordered = ['remote-build', 'returned', 'verify', 'codesign', 'notarize', 'publish', 'cleanup']
                    self.assertEqual(sorted(ordered, key=events.index), ordered)
                    manifests = list(root.rglob('*-manifest.json'))
                    self.assertEqual(json.loads(manifests[0].read_text())['remote_builder'], intel.DEFAULT_REMOTE)


if __name__ == '__main__':
    unittest.main()
