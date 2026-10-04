"""Offline version updates, exact release selection, and atomic Git pushes."""
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts'))
import prepare_release
import release_target
import update_version


def git(root, *args):
    return subprocess.check_output(['git', *args], cwd=root, text=True,
                                   stderr=subprocess.STDOUT).strip()


class VersionFixture(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name)
        git(self.root, 'init', '--initial-branch=main')
        git(self.root, 'config', 'user.name', 'Release Test')
        git(self.root, 'config', 'user.email', 'test@example.invalid')
        for name in (*update_version.CURRENT_PATHS, 'VERSION', 'rack/plugin.json'):
            path = self.root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes((ROOT / name).read_bytes())
        # Fixtures are independent of whichever version the repository currently uses.
        current = (self.root / 'VERSION').read_text().strip()
        for name in update_version.CURRENT_PATHS:
            path = self.root / name
            path.write_text(update_version.version_pattern(current).sub('8.0.5', path.read_text()))
        (self.root / 'VERSION').write_text('8.0.5\n')
        git(self.root, 'add', '.')
        git(self.root, '-c', 'commit.gpgsign=false', 'commit', '-m', 'fixture')

    def add(self, name, text):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)
        git(self.root, 'add', name)



class VersionTests(VersionFixture):
    def test_all_current_references_and_future_bumps(self):
        update_version.update(self.root, '8.0.6')
        update_version.update(self.root, '8.1.0')
        self.assertEqual((self.root / 'VERSION').read_text(), '8.1.0\n')
        for name in update_version.CURRENT_PATHS:
            text = (self.root / name).read_text()
            self.assertIn('8.1.0', text)
            self.assertNotIn('8.0.5', text)
            self.assertNotIn('8.0.6', text)
        self.assertIn('version=v8.1.0', (self.root / 'test/midi/test_midi.c').read_text())

    def test_invalid_versions_do_not_change_files(self):
        before = {p: (self.root / p).read_bytes() for p in update_version.CURRENT_PATHS}
        for version in ('v8.0.6', '08.0.6', '8.00.6', '8.0', '8.0.6-rc1',
                        '8.0.6\n', '256.0.0', '8.0.256', '8.0.5', '7.9.9', '8.0.6; echo bad'):
            with self.subTest(version=version), self.assertRaises(ValueError):
                update_version.update(self.root, version)
        self.assertEqual(before, {p: (self.root / p).read_bytes() for p in before})

    def test_missed_reference_fails_before_writing(self):
        self.add('new_version_response.c', 'version=v8.0.5\n')
        with self.assertRaisesRegex(ValueError, 'new_version_response.c'):
            update_version.update(self.root, '8.0.6')
        self.assertEqual((self.root / 'VERSION').read_text(), '8.0.5\n')

    def test_history_independent_versions_and_binary_files_are_preserved(self):
        self.add('test/rack/test_release.py', 'historical fixture v8.0.5\n')
        self.add('dependency.txt', 'SDK 18.0.5\nprevious release v8.0.4\n')
        self.add('binary.dat', '\0v8.0.5\n')
        plugin = (self.root / 'rack/plugin.json').read_bytes()
        update_version.update(self.root, '8.0.6')
        self.assertEqual((self.root / 'rack/plugin.json').read_bytes(), plugin)
        self.assertEqual((self.root / 'test/rack/test_release.py').read_text(), 'historical fixture v8.0.5\n')
        self.assertIn('18.0.5', (self.root / 'dependency.txt').read_text())

    def test_preview_changes_nothing(self):
        update_version.update(self.root, '8.0.6', check=True)
        self.assertEqual(git(self.root, 'status', '--porcelain'), '')

    def test_rack_links_use_manifest_and_new_release_tag(self):
        path = self.root / 'rack/plugin.json'
        manifest = json.loads(path.read_text())
        manifest.update(slug='OtherPlugin', version='2.3.4')
        path.write_text(json.dumps(manifest))
        update_version.update(self.root, '8.0.6')
        readme = (self.root / 'README.md').read_text()
        for _, platform in update_version.RACK_PLATFORMS:
            self.assertIn('releases/download/v8.0.6/OtherPlugin-2.3.4-' + platform + '.zip', readme)
        for platform in ('macos-arm64', 'macos-x86_64', 'windows-x64', 'linux-x86_64'):
            self.assertIn('releases/download/v8.0.6/_core-sample-manager-8.0.6-' + platform, readme)
        self.assertIn('releases/download/v8.0.6/ectocore_v8.0.6.uf2', readme)

    def test_missing_table_or_current_reference_fails(self):
        path = self.root / 'README.md'
        path.write_text(path.read_text().replace(update_version.RACK_START, ''))
        with self.assertRaisesRegex(ValueError, 'marked Rack'):
            update_version.update(self.root, '8.0.6')


class GitReleaseTests(VersionFixture):
    def setUp(self):
        super().setUp()
        self.remote = self.root.parent / (self.root.name + '-remote.git')
        self.addCleanup(lambda: __import__('shutil').rmtree(self.remote, ignore_errors=True))
        git(self.root, 'init', '--bare', str(self.remote))
        git(self.root, 'remote', 'add', 'origin', str(self.remote))
        git(self.root, 'push', '-u', 'origin', 'main')
        git(self.remote, 'symbolic-ref', 'HEAD', 'refs/heads/main')
        self.before = git(self.root, 'rev-parse', 'HEAD')

    def prepare(self, **kwargs):
        return prepare_release.prepare(self.root, '8.0.6', 'example/repo',
                                       check_release=lambda *_: None, **kwargs)

    def test_commit_and_tag_identify_same_updated_source(self):
        result = self.prepare()
        self.assertEqual(result['version'], '8.0.6')
        self.assertEqual(git(self.remote, 'rev-parse', 'main'), result['source_sha'])
        self.assertEqual(git(self.remote, 'rev-parse', 'v8.0.6'), result['source_sha'])
        self.assertEqual(git(self.root, 'log', '-1', '--format=%s'), 'chore: release v8.0.6')
        self.assertEqual(git(self.remote, 'show', 'main:VERSION'), '8.0.6')
        self.assertEqual(git(self.root, 'status', '--porcelain'), '')

    def test_existing_tag_rejected_without_commit(self):
        git(self.remote, 'tag', 'v8.0.6', self.before)
        with self.assertRaisesRegex(ValueError, 'already exists'):
            self.prepare()
        self.assertEqual(git(self.root, 'rev-parse', 'HEAD'), self.before)

    def test_existing_release_rejected_without_commit(self):
        with patch.object(prepare_release.subprocess, 'check_output', return_value='v8.0.6\n'):
            with self.assertRaisesRegex(ValueError, 'release already exists'):
                prepare_release.release_absent('example/repo', 'v8.0.6')

    def test_api_failure_is_not_treated_as_absent_release(self):
        with patch.object(prepare_release.subprocess, 'check_output',
                          side_effect=subprocess.CalledProcessError(1, 'gh')):
            with self.assertRaises(subprocess.CalledProcessError):
                prepare_release.release_absent('example/repo', 'v8.0.6')

    def test_concurrent_main_change_rejects_both_refs(self):
        def advance():
            tree = git(self.remote, 'rev-parse', 'main^{tree}')
            new = git(self.root, '-c', 'commit.gpgsign=false', 'commit-tree', tree,
                      '-p', self.before, '-m', 'concurrent change')
            git(self.root, 'push', 'origin', new + ':refs/heads/main')
        with self.assertRaises(subprocess.CalledProcessError):
            self.prepare(before_push=advance)
        self.assertNotEqual(git(self.remote, 'rev-parse', 'main'), self.before)
        self.assertEqual(git(self.remote, 'tag', '--list', 'v8.0.6'), '')

    def test_concurrent_tag_creation_rejects_both_refs(self):
        def create_tag():
            git(self.remote, 'tag', 'v8.0.6', self.before)
        with self.assertRaises(subprocess.CalledProcessError):
            self.prepare(before_push=create_tag)
        self.assertEqual(git(self.remote, 'rev-parse', 'main'), self.before)
        self.assertEqual(git(self.remote, 'rev-parse', 'v8.0.6'), self.before)

    def test_dirty_checkout_rejected(self):
        (self.root / 'VERSION').write_text('edited\n')
        with self.assertRaisesRegex(ValueError, 'clean checkout'):
            self.prepare()


class TargetTests(unittest.TestCase):
    def test_paired_inputs_and_stable_versions(self):
        for tag, sha in (('v8.0.6', ''), ('', 'a' * 40), ('8.0.6', 'a' * 40),
                         ('v8.0.6-rc1', 'a' * 40), ('v8.0.6', 'abcdef')):
            with self.subTest(tag=tag, sha=sha), self.assertRaises(ValueError):
                release_target.validate_target(tag, sha)
        release_target.validate_target('', '')
        release_target.validate_target('v8.0.6', 'a' * 40)

    def test_lightweight_and_annotated_tags_resolve_to_commit(self):
        from unittest.mock import Mock
        runner = Mock()
        runner.run.return_value = 'a' * 40 + '\trefs/tags/v8.0.6'
        release_target.verify_tag(runner, 'fixture', 'v8.0.6', 'a' * 40)
        runner.run.return_value = ('b' * 40 + '\trefs/tags/v8.0.6\n'
                                   + 'a' * 40 + '\trefs/tags/v8.0.6^{}')
        release_target.verify_tag(runner, 'fixture', 'v8.0.6', 'a' * 40)
        with self.assertRaisesRegex(ValueError, 'does not point'):
            release_target.verify_tag(runner, 'fixture', 'v8.0.6', 'c' * 40)


if __name__ == '__main__':
    unittest.main()
