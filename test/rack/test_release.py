"""Offline release checks; no native builds, SSH connections or live uploads."""
import hashlib
import io
import json
import struct
import subprocess
import sys
import tarfile
import tempfile
import unittest
import zipfile
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'rack/Release'))
import rack_package
import rack_release


class FakeRunner:
    def __init__(self):
        self.release = {'id': 123, 'tag_name': 'v8.0.4', 'html_url': 'https://example.invalid/v8.0.4'}
        self.latest = None
        self.commands = []
        self.uploaded = {}
        self.corrupt = False

    def write(self, value):
        pass

    def run(self, *args, **kwargs):
        args = list(map(str, args))
        self.commands.append(args)
        if args[0] == 'git':
            return subprocess.check_output(args, text=True, stderr=subprocess.STDOUT).strip()
        if args[:2] == ['gh', 'api']:
            return json.dumps((self.latest or self.release) if args[2].endswith('/latest') else self.release)
        if args[:3] == ['gh', 'release', 'upload']:
            self.uploaded = {Path(p).name: Path(p).read_bytes() for p in args[args.index('--') + 2:]}
        elif args[:3] == ['gh', 'release', 'download']:
            name = args[args.index('--pattern') + 1]
            content = b'corrupt' if self.corrupt else self.uploaded[name]
            (Path(args[args.index('--dir') + 1]) / name).write_bytes(content)
        elif args[0] == 'otool':
            return '/tmp/Rack2/libRack.dylib (compatibility version 0.0.0)'
        elif args[0] not in ('codesign', 'readelf'):
            raise AssertionError('Unexpected command: ' + repr(args))
        return ''


def tar_bytes(files):
    stream = io.BytesIO()
    with tarfile.open(fileobj=stream, mode='w') as archive:
        for name, data in files.items():
            info = tarfile.TarInfo(name)
            info.size = len(data)
            archive.addfile(info, io.BytesIO(data))
    return stream.getvalue()


def binary(platform_name):
    data = bytearray(64)
    if platform_name.startswith('mac-'):
        cpu = 0x0100000c if platform_name == 'mac-arm64' else 0x01000007
        struct.pack_into('<IIII', data, 0, 0xfeedfacf, cpu, 0, 6)
    else:
        data[:6] = b'\x7fELF\x02\x01'
        struct.pack_into('<HH', data, 16, 3, 62)
    return bytes(data)


class ReleaseTests(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name)
        self.runner = FakeRunner()
        self.selection = {'release': rack_release.latest_release(self.runner),
                          'source': {'repository': rack_release.REPOSITORY,
                                     'ref': 'refs/heads/main', 'commit': 'a' * 40}}

    def make_assets(self):
        output = self.root / 'assets'
        output.mkdir()
        archive = output / 'InfiniteDigits-2.0.0-mac-arm64.zip'
        archive.write_bytes(b'fixture')
        checksums = output / 'InfiniteDigits-2.0.0-mac-arm64-SHA256SUMS.txt'
        checksums.write_text(f'{rack_package.sha(archive)}  {archive.name}\n')
        return [archive, checksums]

    def test_publish_downloads_both_assets_before_marking_complete(self):
        assets = self.make_assets()
        rack_release.publish(self.runner, self.selection, assets)
        complete = json.loads((assets[0].parent / 'complete.json').read_text())
        self.assertEqual(complete['source']['commit'], 'a' * 40)
        self.assertEqual(complete['assets'], {p.name: rack_package.sha(p) for p in assets})
        self.assertEqual(len(self.runner.uploaded), 2)

    def test_changed_release_aborts_without_upload(self):
        assets = self.make_assets()
        self.runner.release['id'] += 1
        with self.assertRaisesRegex(RuntimeError, 'changed'):
            rack_release.publish(self.runner, self.selection, assets)
        self.assertFalse(self.runner.uploaded)
        self.assertFalse((assets[0].parent / 'complete.json').exists())

    def test_immutable_or_unpublished_releases_abort(self):
        for flag in ('immutable', 'draft', 'prerelease'):
            with self.subTest(flag=flag), patch.dict(self.runner.release, {flag: True}):
                with self.assertRaisesRegex(RuntimeError, 'stable and mutable'):
                    rack_release.latest_release(self.runner)

    def test_corrupt_download_does_not_mark_complete(self):
        assets = self.make_assets()
        self.runner.corrupt = True
        with self.assertRaisesRegex(RuntimeError, 'verification failed'):
            rack_release.publish(self.runner, self.selection, assets)
        self.assertFalse((assets[0].parent / 'complete.json').exists())

    def test_pinned_clone_ignores_later_main_and_release_tag(self):
        upstream = self.root / 'upstream'
        def git(*args):
            return subprocess.check_output(['git', *args], stderr=subprocess.STDOUT, text=True).strip()
        git('init', '--initial-branch=main', str(upstream))
        git('-C', str(upstream), 'config', 'user.name', 'Release Test')
        git('-C', str(upstream), 'config', 'user.email', 'test@example.invalid')
        project = upstream / 'rack'
        project.mkdir()
        manifest = project / 'plugin.json'
        manifest.write_text('{"slug":"InfiniteDigits","version":"2.0.0"}')
        git('-C', str(upstream), 'add', '.')
        git('-C', str(upstream), '-c', 'commit.gpgsign=false', 'commit', '-m', 'initial')
        with patch.object(rack_release, 'REPOSITORY_URL', upstream.as_uri()):
            selection = rack_release.select_release(self.runner)
            manifest.write_text('{"slug":"InfiniteDigits","version":"2.1.0"}')
            git('-C', str(upstream), 'add', '.')
            git('-C', str(upstream), '-c', 'commit.gpgsign=false', 'commit', '-m', 'advance main')
            clone = self.root / 'clone'
            rack_release.clone_source(self.runner, selection, clone)
        self.assertEqual(rack_package.read_plugin(clone)['version'], '2.0.0')
        self.assertEqual(git('-C', str(clone), 'rev-parse', 'HEAD'), selection['source']['commit'])
        self.assertEqual(selection['release']['tag'], 'v8.0.4')

    def test_explicit_upload_uses_named_release_even_if_latest_changes(self):
        self.selection['source']['ref'] = 'refs/tags/v8.0.4'
        self.runner.latest = {**self.runner.release, 'id': 999, 'tag_name': 'v8.0.7'}
        assets = self.make_assets()
        with patch.object(rack_release, 'verify_tag') as verify:
            rack_release.publish(self.runner, self.selection, assets)
        verify.assert_called_once_with(self.runner, rack_release.REPOSITORY_URL, 'v8.0.4', 'a' * 40)
        upload = next(c for c in self.runner.commands if c[:3] == ['gh', 'release', 'upload'])
        self.assertEqual(upload[upload.index('--') + 1], 'v8.0.4')

    def test_explicit_selection_requires_matching_tag_and_commit(self):
        with self.assertRaisesRegex(ValueError, 'together'):
            rack_release.select_release(self.runner, 'v8.0.4')
        with patch.object(rack_release, 'verify_tag', side_effect=ValueError('tag mismatch')):
            with self.assertRaisesRegex(ValueError, 'tag mismatch'):
                rack_release.select_release(self.runner, 'v8.0.4', 'a' * 40)
        with patch.object(rack_release, 'verify_tag'):
            selection = rack_release.select_release(self.runner, 'v8.0.4', 'a' * 40)
        self.assertEqual(selection['source']['ref'], 'refs/tags/v8.0.4')
        self.assertEqual(selection['source']['commit'], 'a' * 40)

    def test_intel_shell_is_valid_and_pins_the_source(self):
        script = rack_release.remote_build_script('/tmp/core-rack-intel.fixture', self.selection, 3)
        subprocess.run(['bash', '-n'], input=script, text=True, check=True)
        self.assertIn('checkout --detach ' + 'a' * 40, script)
        self.assertIn('mac-x64', script)
        self.assertIn('CXX=clang++', script)
        self.assertIn('caffeinate -i', script)
        self.assertNotIn('gh release', script)
        self.assertNotIn('notarytool', script)

    def test_all_build_plans_use_target_sdk_and_no_install_or_runtime_tests(self):
        for target in ('mac-arm64', 'mac-x64', 'lin-x64'):
            with self.subTest(target=target):
                commands = rack_release.build_commands(self.root, target, 4, '/usr/bin/python3')
                self.assertIn(target, commands[1])
                self.assertEqual(commands[2][-1], 'dist')
                self.assertIn('CC=gcc' if target == 'lin-x64' else 'CC=clang', commands[2])
                self.assertIn('PYTHON=/usr/bin/python3', commands[2])
                self.assertFalse(any('install' in c or 'test' in c for c in commands))

    def test_native_architectures_and_wrong_binary_types(self):
        for target in ('mac-arm64', 'mac-x64', 'lin-x64'):
            rack_package.verify_binary(binary(target), target)
            for other in {'mac-arm64', 'mac-x64', 'lin-x64'} - {target}:
                with self.assertRaises(ValueError):
                    rack_package.verify_binary(binary(other), target)
            with self.assertRaises(ValueError):
                rack_package.verify_binary(b'bad', target)

    def package_fixture(self, target):
        project = self.root / 'rack'
        project.mkdir()
        manifest = {'slug': 'InfiniteDigits', 'version': '2.0.0',
                    'name': 'Infinite Digits', 'manualUrl': 'https://example.invalid/manual'}
        files = {'rack/plugin.json': json.dumps(manifest).encode()}
        for name in ('rack/Makefile', 'lib/core_engine/prepare.py', 'lib/crossfade4.py',
                     'lib/fuzz.py', 'lib/resonantfilter.py', 'main.c', 'rack/README.md',
                     'rack/LICENSE.txt', 'rack/THIRD_PARTY.md', 'rack/res/panel.svg', 'rack/licenses/notice.txt'):
            files[name] = (name + '\n').encode()
        for path, data in files.items():
            file = self.root / path
            file.parent.mkdir(parents=True, exist_ok=True)
            file.write_bytes(data)
        hashes = {path: hashlib.sha256(data).hexdigest() for path, data in files.items()}
        source_files = {f'InfiniteDigits-2.0.0-source/{path}': data for path, data in files.items()}
        source_files['InfiniteDigits-2.0.0-source/SOURCE_SHA256.json'] = json.dumps(hashes).encode()
        dist = project / 'dist'
        dist.mkdir()
        source = dist / 'InfiniteDigits-2.0.0-source.tar.gz'
        source.write_bytes(tar_bytes(source_files))
        plugin_files = {f'InfiniteDigits/{path[5:]}': data for path, data in files.items() if path.startswith('rack/')}
        extension = 'so' if target == 'lin-x64' else 'dylib'
        plugin_files['InfiniteDigits/plugin.' + extension] = binary(target)
        (dist / f'InfiniteDigits-2.0.0-{target}.vcvplugin').write_bytes(b'compressed fixture')
        return dist, source, plugin_files

    def test_package_keeps_plugin_version_and_records_release_separately(self):
        dist, _, files = self.package_fixture('mac-arm64')
        with patch.object(rack_package.subprocess, 'check_output', return_value=tar_bytes(files)):
            assets = rack_package.package(self.root, dist, self.root / 'assets', 'mac-arm64',
                                          self.selection, self.runner, 'fixture-host')
        self.assertEqual(assets[0].name, 'InfiniteDigits-2.0.0-mac-arm64.zip')
        with zipfile.ZipFile(assets[0]) as archive:
            prefix = 'InfiniteDigits-2.0.0-mac-arm64/'
            build = json.loads(archive.read(prefix + 'BUILD.json'))
            self.assertEqual(build['version'], '2.0.0')
            self.assertEqual(build['release']['tag'], 'v8.0.4')
            self.assertFalse(build['runtime_tests_run'])
            for line in archive.read(prefix + 'SHA256SUMS.txt').decode().splitlines():
                expected, name = line.split('  ')
                self.assertEqual(hashlib.sha256(archive.read(prefix + name)).hexdigest(), expected)
        self.assertFalse((assets[0].parent / 'complete.json').exists())

    def test_source_mismatch_aborts_packaging(self):
        _, source, _ = self.package_fixture('mac-x64')
        (self.root / 'main.c').write_text('changed source')
        with self.assertRaisesRegex(ValueError, 'Source archive mismatch'):
            rack_package.verify_source(self.root, source, 'InfiniteDigits', '2.0.0')

    def test_resource_mismatch_aborts_packaging(self):
        dist, _, files = self.package_fixture('lin-x64')
        files['InfiniteDigits/res/panel.svg'] = b'wrong panel'
        with patch.object(rack_package.subprocess, 'check_output', return_value=tar_bytes(files)):
            with self.assertRaisesRegex(ValueError, 'resource mismatch'):
                rack_package.package(self.root, dist, self.root / 'assets', 'lin-x64',
                                     self.selection, self.runner, 'fixture-host')


if __name__ == '__main__':
    unittest.main()
