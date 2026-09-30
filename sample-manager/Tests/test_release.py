"""Offline release regression tests: real source clones, simulated native builds/uploads."""
import importlib.util
import json
import os
import subprocess
import sys
import tempfile
import unittest
from contextlib import ExitStack
from pathlib import Path
from unittest.mock import patch

RELEASE_TOOLS = Path(__file__).resolve().parents[1] / 'Release'
sys.path.insert(0, str(RELEASE_TOOLS))
import native_release
import release_common as common

spec = importlib.util.spec_from_file_location('release_windows', RELEASE_TOOLS / 'release-windows.py')
windows = importlib.util.module_from_spec(spec)
spec.loader.exec_module(windows)


class FixtureRunner(common.Runner):
    """Run Git locally while capturing all build, signing and publication boundaries."""
    def __init__(self, release):
        super().__init__()
        self.release = release
        self.commands = []
        self.uploaded = []
        self.messages = []
        self.remote_script = ''

    def write(self, value):
        self.messages.append(value)

    def run(self, *args):
        args = list(map(str, args))
        self.commands.append(args)
        if args[0] == 'git':
            return super().run(*args)
        if args[:3] == ['gh', 'api', f'repos/{common.REPOSITORY}/releases/latest']:
            return json.dumps(self.release)
        if args[:3] == ['gh', 'release', 'upload']:
            self.uploaded = list(map(Path, args[4:args.index('--repo')]))
            return ''
        if args[:3] == ['gh', 'api', f"repos/{common.REPOSITORY}/releases/{self.release['id']}"]:
            return json.dumps({'assets': [
                {'name': asset.name, 'size': asset.stat().st_size, 'digest': 'sha256:' + common.sha(asset)}
                for asset in self.uploaded
            ]})
        if args[0] in ('cmake', '/usr/bin/caffeinate'):
            return ''
        if args[:2] == ['readelf', '-h']:
            return 'Class: ELF64\nMachine: Advanced Micro Devices X86-64'
        if args[0] == 'powershell':
            if '-OutputDirectory' in args:
                output = Path(args[args.index('-OutputDirectory') + 1])
                output.mkdir()
                version = args[args.index('-Version') + 1]
                (output / f'_core-sample-manager-{version}-windows-x64.zip').write_bytes(b'windows package')
                common.write_json(output / 'manifest.json', {'signature': {
                    'status': 'Valid', 'timestampCertificate': 'timestamp',
                    'publisher': 'publisher', 'certificate': 'certificate',
                }})
            return ''
        raise AssertionError('Unexpected command: ' + repr(args))

    def ssh(self, host, script):
        if script.startswith('mktemp '):
            return '/tmp/core-sample-manager-intel.fixture'
        if script.startswith('rm -rf '):
            return ''
        self.remote_script = script
        subprocess.run(['bash', '-n'], input=script, text=True, check=True)
        return ''


class ReleaseTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.repository = self.root / 'upstream'
        self.git('init', '--initial-branch=main', str(self.repository))
        self.git('-C', str(self.repository), 'config', 'user.email', 'release-test@example.invalid')
        self.git('-C', str(self.repository), 'config', 'user.name', 'Release Test')
        (self.repository / 'README.md').write_text('release predates the app\n')
        self.commit('old release')
        self.git('-C', str(self.repository), 'tag', 'v8.0.2')
        project = self.repository / 'sample-manager'
        project.mkdir()
        (project / 'CMakeLists.txt').write_text('PRODUCT_NAME "_core sample manager"\n')
        self.source_commit = self.commit('add app on main')
        self.release = {'id': 123, 'tag_name': 'v8.0.2', 'html_url': 'https://example.invalid/releases/v8.0.2',
                        'draft': False, 'prerelease': False, 'immutable': False}
        self.runner = FixtureRunner(self.release)
        repository_url = patch.object(common, 'REPOSITORY_URL', self.repository.as_uri())
        repository_url.start()
        self.addCleanup(repository_url.stop)

    @staticmethod
    def git(*args):
        return subprocess.check_output(['git', *args], text=True, stderr=subprocess.STDOUT).strip()

    def commit(self, message):
        self.git('-C', str(self.repository), 'add', '.')
        self.git('-C', str(self.repository), '-c', 'commit.gpgsign=false', 'commit', '-m', message)
        return self.git('-C', str(self.repository), 'rev-parse', 'HEAD')

    def advance_main(self):
        (self.repository / 'sample-manager/CMakeLists.txt').write_text('PRODUCT_NAME "Next App"\n')
        return self.commit('advance main')

    def package(self, selection, platform_name='linux-x86_64'):
        output = self.root / 'assets'
        output.mkdir()
        archive = output / f"_core-sample-manager-{selection['release']['version']}-{platform_name}.tar.gz"
        archive.write_bytes(b'package fixture')
        common.finish_package(output, archive, {}, selection, platform_name)
        return output

    def test_main_source_is_independent_of_release_tag_and_later_branch_updates(self):
        selection = common.select_release(self.runner)
        self.assertEqual(selection['release']['version'], '8.0.2')
        self.assertEqual(selection['source']['ref'], 'refs/heads/main')
        self.assertEqual(selection['source']['commit'], self.source_commit)
        self.assertNotEqual(self.advance_main(), self.source_commit)
        project = common.clone_source(self.runner, selection, self.root / 'clone')
        self.assertEqual((project / 'CMakeLists.txt').read_text(), 'PRODUCT_NAME "_core sample manager"\n')
        self.assertEqual(self.git('-C', str(project), 'rev-parse', 'HEAD'), self.source_commit)

    def test_publish_uses_release_version_and_destination_after_main_advances(self):
        selection = common.select_release(self.runner)
        output = self.package(selection)
        self.advance_main()
        common.publish(self.runner, output, 'linux-x86_64')
        upload = next(command for command in self.runner.commands if command[:3] == ['gh', 'release', 'upload'])
        self.assertEqual(upload[3], 'v8.0.2')
        self.assertIn('--clobber', upload)
        self.assertEqual(len(self.runner.uploaded), 3)
        self.assertTrue(all(asset.name.startswith('_core-sample-manager-8.0.2-linux-x86_64')
                            for asset in self.runner.uploaded))
        report = common.read_json(output / '_core-sample-manager-8.0.2-linux-x86_64-manifest.json')
        self.assertEqual(report['release'], selection['release'])
        self.assertEqual(report['source']['commit'], self.source_commit)
        self.assertEqual(common.read_json(output / 'complete.json')['source'], selection['source'])

    def test_new_release_blocks_upload_and_retains_artifacts(self):
        selection = common.select_release(self.runner)
        output = self.package(selection)
        self.release.update(id=124, tag_name='v8.0.3')
        with self.assertRaisesRegex(common.ReleaseError, 'Latest release changed'):
            common.publish(self.runner, output, 'linux-x86_64')
        self.assertFalse(self.runner.uploaded)
        self.assertFalse((output / 'complete.json').exists())
        self.assertEqual(len(list(output.iterdir())), 3)

    def test_immutable_release_blocks_upload(self):
        selection = common.select_release(self.runner)
        output = self.package(selection)
        self.release['immutable'] = True
        with self.assertRaisesRegex(common.ReleaseError, 'immutable'):
            common.publish(self.runner, output, 'linux-x86_64')
        self.assertFalse(self.runner.uploaded)

    def test_invalid_release_version_is_rejected(self):
        for tag in ('nightly', 'v256.0.0', 'v8.0.2-rc1'):
            with self.subTest(tag=tag):
                self.release['tag_name'] = tag
                with self.assertRaises(common.ReleaseError):
                    common.select_release(self.runner)

    def test_selected_source_must_contain_application(self):
        (self.repository / 'sample-manager/CMakeLists.txt').unlink()
        self.commit('remove app')
        selection = common.select_release(self.runner)
        with self.assertRaisesRegex(common.ReleaseError, 'main commit does not contain'):
            common.clone_source(self.runner, selection, self.root / 'clone')

    def test_native_entrypoints_build_main_with_release_version(self):
        for platform_name in ('macos-arm64', 'macos-x86_64', 'linux-x86_64'):
            for no_upload in (False, True):
                with self.subTest(platform=platform_name, no_upload=no_upload), ExitStack() as stack:
                    runner = FixtureRunner(self.release)
                    output = self.root / (platform_name + ('-local' if no_upload else '-publish'))
                    output.mkdir()
                    package_versions = []

                    def package_build(project, build, assets, platform, version, **kwargs):
                        self.assertEqual(self.git('-C', str(project), 'rev-parse', 'HEAD'), self.source_commit)
                        package_versions.append(version)
                        assets.mkdir()
                        (assets / '_core sample manager').mkdir()
                        extension = '.tar.gz' if platform.startswith('linux') else '.zip'
                        archive = assets / f'_core-sample-manager-{version}-{platform}{extension}'
                        archive.write_bytes(b'native package')
                        return archive, {'signing': 'Developer ID', 'notarization': {'status': 'Accepted'}}

                    stack.enter_context(patch.object(native_release, 'Runner', return_value=runner))
                    stack.enter_context(patch.object(native_release, 'require_tools'))
                    stack.enter_context(patch.object(native_release, 'new_run', return_value=output))
                    stack.enter_context(patch.object(native_release, 'package_build', side_effect=package_build))
                    stack.enter_context(patch.object(native_release, 'mac_credentials', return_value=('identity', [])))
                    stack.enter_context(patch.object(native_release, 'retrieve_intel', return_value=None))
                    stack.enter_context(patch.object(native_release.platform, 'system', return_value='Linux' if platform_name.startswith('linux') else 'Darwin'))
                    stack.enter_context(patch.object(native_release.platform, 'machine', return_value='arm64' if platform_name == 'macos-arm64' else 'x86_64'))
                    stack.enter_context(patch.object(sys, 'argv', ['release'] + (['--no-upload'] if no_upload else [])))
                    self.assertEqual(native_release.main(platform_name), 0, runner.messages)
                    self.assertEqual(package_versions, ['8.0.2'])
                    if platform_name == 'macos-x86_64':
                        self.assertIn('--branch main', runner.remote_script)
                        self.assertIn('checkout --detach ' + self.source_commit, runner.remote_script)
                        self.assertIn('-DCORE_MANAGER_VERSION=8.0.2', runner.remote_script)
                    else:
                        self.assertTrue(any('-DCORE_MANAGER_VERSION=8.0.2' in command for command in runner.commands))
                    self.assertEqual(bool(runner.uploaded), not no_upload)
                    self.assertEqual((output / 'assets/complete.json').exists(), not no_upload)

    def test_windows_prepare_build_package_and_publish(self):
        output = self.root / 'windows'
        output.mkdir()
        github_env = self.root / 'github-env'
        with ExitStack() as stack:
            stack.enter_context(patch.object(windows, 'Runner', return_value=self.runner))
            stack.enter_context(patch.object(windows, 'require_tools'))
            stack.enter_context(patch.object(windows, 'new_run', return_value=output))
            stack.enter_context(patch.object(windows.platform, 'system', return_value='Windows'))
            stack.enter_context(patch.dict(os.environ, {'GITHUB_ENV': str(github_env), 'RUNNER_TEMP': str(self.root),
                                                        'RUNNER_TOOL_CACHE': str(self.root / 'cache')}))
            with patch.object(sys, 'argv', ['release-windows', 'prepare']):
                self.assertEqual(windows.main(), 0, self.runner.messages)
            environment = dict(line.split('=', 1) for line in github_env.read_text().splitlines())
            self.assertEqual(environment['CORE_RELEASE_VERSION'], '8.0.2')
            self.advance_main()
            stack.enter_context(patch.dict(os.environ, environment))
            for operation in ('build', 'package'):
                with patch.object(sys, 'argv', ['release-windows', operation]):
                    self.assertEqual(windows.main(), 0, self.runner.messages)
            for command in (command for command in self.runner.commands if command[0] == 'powershell'):
                self.assertEqual(command[command.index('-Version') + 1], '8.0.2')
            with patch.object(sys, 'argv', ['release-windows', 'publish', '--assets', str(output / 'assets')]):
                self.assertEqual(windows.main(), 0, self.runner.messages)
            report = common.read_json(output / 'assets/_core-sample-manager-8.0.2-windows-x64-manifest.json')
            self.assertEqual(report['source']['commit'], self.source_commit)
            self.assertEqual(report['version'], '8.0.2')
            self.assertEqual(len(self.runner.uploaded), 3)


if __name__ == '__main__':
    unittest.main()
