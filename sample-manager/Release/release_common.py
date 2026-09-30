"""Release selection, clean source clones, evidence and existing-release uploads.

These helpers come from the invoking checkout. Application source always comes
from the selected release tag, which need not contain these helpers.
"""
from __future__ import annotations

import json
import os
import re
import shlex
import shutil
import subprocess
import tempfile
from datetime import datetime, timezone
from pathlib import Path

from package import sha

REPOSITORY = 'schollz/_core'
REPOSITORY_URL = 'https://github.com/' + REPOSITORY + '.git'
ROOT = Path(__file__).resolve().parents[1]
VERSION = re.compile(r'v?((?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*))')
SSH = ['ssh', '-o', 'BatchMode=yes', '-o', 'ForwardAgent=no', '-o', 'ConnectTimeout=10']
PRIVATE_ENV = ('APPLE_ID', 'TEAM_ID', 'APPLE_PASSWORD', 'SIGN_IDENTITY',
               'INSTALLER_SIGN_IDENTITY', 'NOTARY_PROFILE', 'GH_TOKEN', 'GITHUB_TOKEN')


class ReleaseError(RuntimeError):
    pass


class Runner:
    def __init__(self, log=None):
        self.log = log
        self.secrets = [os.environ[key] for key in
                        ('APPLE_PASSWORD', 'GH_TOKEN', 'GITHUB_TOKEN', 'AZURE_CLIENT_SECRET')
                        if os.environ.get(key)]

    def redact(self, value):
        for secret in self.secrets:
            value = value.replace(secret, '<redacted>')
        return value

    def write(self, value):
        value = self.redact(value)
        print(value, flush=True)
        if self.log:
            with self.log.open('a', encoding='utf-8') as handle:
                handle.write(value + '\n')

    def run(self, *args):
        args = [str(arg) for arg in args]
        self.write('+ ' + shlex.join(args))
        environment = os.environ.copy()
        environment['LC_ALL'] = 'C'
        if args[0] in ('ssh', 'rsync', 'git', 'cmake', '/usr/bin/caffeinate'):
            for key in PRIVATE_ENV:
                environment.pop(key, None)
        # Stream compiler output to the console and retained log. Returned output
        # is also redacted so callers cannot accidentally write credentials.
        with subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                              text=True, encoding='utf-8', errors='replace', env=environment) as process:
            lines = []
            for line in process.stdout:
                line = self.redact(line.rstrip('\n'))
                lines.append(line)
                self.write(line)
            code = process.wait()
        output = '\n'.join(lines)
        if code:
            raise subprocess.CalledProcessError(code, args, output=output)
        return output.strip()

    def ssh(self, host, script):
        return self.run(*SSH, host, '/bin/bash -c ' + shlex.quote('set -euo pipefail\n' + script))


def require_tools(*names):
    missing = [name for name in names if not shutil.which(name)]
    if missing:
        raise ReleaseError('Install required tools first: ' + ', '.join(missing))


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8')


def read_json(path):
    # PowerShell 5.1 writes UTF-8 with a BOM.
    return json.loads(path.read_text(encoding='utf-8-sig'))


def latest_release(runner):
    try:
        release = json.loads(runner.run('gh', 'api', f'repos/{REPOSITORY}/releases/latest'))
    except subprocess.CalledProcessError as error:
        raise ReleaseError('Cannot read the latest published release. Confirm a stable release exists '
                           'and gh is authenticated; the API error is in the log.') from error
    tag = release.get('tag_name', '')
    match = VERSION.fullmatch(tag)
    if not match or release.get('draft') or release.get('prerelease'):
        raise ReleaseError('Latest release must be a published stable vMAJOR.MINOR.PATCH release')
    if release.get('immutable'):
        raise ReleaseError('Latest release is immutable; its assets cannot be replaced')
    # JUCE encodes version components in one byte in its numeric version code.
    if any(int(part) > 255 for part in match.group(1).split('.')):
        raise ReleaseError('Release version components must be at most 255 for JUCE')
    return {'repository': REPOSITORY, 'id': release['id'], 'tag': tag,
            'version': match.group(1), 'url': release['html_url']}


def tag_commit(runner, tag):
    ref = 'refs/tags/' + tag
    output = runner.run('git', 'ls-remote', '--exit-code', '--tags', REPOSITORY_URL, ref, ref + '^{}')
    refs = dict(line.split()[::-1] for line in output.splitlines() if len(line.split()) == 2)
    commit = refs.get(ref + '^{}', refs.get(ref, ''))
    if not re.fullmatch(r'[0-9a-f]{40}', commit):
        raise ReleaseError('Cannot resolve the latest release tag to a commit')
    return commit


def select_release(runner):
    selection = latest_release(runner)
    selection['commit'] = tag_commit(runner, selection['tag'])
    return selection


def new_run(parent, platform_name):
    parent.mkdir(parents=True, exist_ok=True)
    return Path(tempfile.mkdtemp(prefix=platform_name + '-', dir=str(parent.resolve())))


def clone_release(runner, selection, destination):
    runner.run('git', 'clone', '--config', 'core.longpaths=true', '--depth', '1', '--no-checkout', '--branch', selection['tag'],
               REPOSITORY_URL, destination)
    actual = runner.run('git', '-C', destination, 'rev-parse', 'HEAD')
    if actual != selection['commit']:
        raise ReleaseError('Release tag moved while cloning; refusing different source')
    runner.run('git', '-C', destination, 'checkout', '--detach', selection['commit'])
    project = destination / 'sample-manager'
    if not (project / 'CMakeLists.txt').is_file():
        raise ReleaseError(f"Release {selection['tag']} does not contain Sample Manager. "
                           'Publish a newer release containing sample-manager/ first.')
    return project


def recheck_release(runner, selection):
    if selection.get('repository') != REPOSITORY:
        raise ReleaseError('Unexpected repository in release metadata')
    current = select_release(runner)
    if any(current[key] != selection.get(key) for key in ('id', 'tag', 'commit', 'version')):
        raise ReleaseError('Latest release or its tag changed during the build; upload aborted, artifacts retained')


def asset_prefix(version, platform_name):
    return f'Core-Sample-Manager-{version}-{platform_name}'


def finish_package(output, archive, manifest, selection, platform_name, *, will_publish=True):
    """Write platform-specific metadata without marking the release complete."""
    prefix = asset_prefix(selection['version'], platform_name)
    manifest.update(application='Core Sample Manager', version=selection['version'],
                    platform=platform_name, release=selection,
                    created=datetime.now(timezone.utc).isoformat(),
                    publication='requested; see complete.json for confirmation' if will_publish else 'disabled by --no-upload',
                    applicationTestsRun=False,
                    archive=archive.name, archiveSHA256=sha(archive), archiveBytes=archive.stat().st_size)
    report = output / (prefix + '-manifest.json')
    write_json(report, manifest)
    checksums = output / (prefix + '-SHA256SUMS.txt')
    checksums.write_text(''.join(sha(p) + '  ' + p.name + '\n' for p in (archive, report)), encoding='utf-8')
    return [archive, report, checksums]


def verify_package(output, platform_name):
    reports = list(output.glob(f'Core-Sample-Manager-*-{platform_name}-manifest.json'))
    if len(reports) != 1:
        raise ReleaseError('Expected exactly one release manifest')
    report = reports[0]
    manifest = read_json(report)
    selection = manifest['release']
    prefix = asset_prefix(selection['version'], platform_name)
    extension = '.tar.gz' if platform_name.startswith('linux') else '.zip'
    archive = output / (prefix + extension)
    checksums = output / (prefix + '-SHA256SUMS.txt')
    if report.name != prefix + '-manifest.json' or manifest['platform'] != platform_name:
        raise ReleaseError('Unexpected manifest platform or filename')
    if manifest['archive'] != archive.name or manifest['archiveSHA256'] != sha(archive):
        raise ReleaseError('Archive does not match its release manifest')
    if manifest['archiveBytes'] != archive.stat().st_size:
        raise ReleaseError('Archive size does not match its release manifest')
    expected = ''.join(sha(p) + '  ' + p.name + '\n' for p in (archive, report))
    if checksums.read_text(encoding='utf-8') != expected:
        raise ReleaseError('Transferred package checksums do not match')
    if platform_name == 'windows-x64':
        signature = manifest.get('signature', {})
        if (signature.get('status') != 'Valid' or not signature.get('timestampCertificate')
                or not signature.get('publisher') or not signature.get('certificate')):
            raise ReleaseError('Release metadata lacks a valid timestamped Windows signature')
    elif platform_name.startswith('macos'):
        if manifest.get('signing') != 'Developer ID' or manifest.get('notarization', {}).get('status') != 'Accepted':
            raise ReleaseError('Release metadata lacks Developer ID signing and accepted notarization')
    return selection, [archive, report, checksums]


def publish(runner, output, platform_name):
    selection, assets = verify_package(output, platform_name)
    recheck_release(runner, selection)
    runner.run('gh', 'release', 'upload', selection['tag'], *assets, '--repo', REPOSITORY, '--clobber')
    uploaded = json.loads(runner.run('gh', 'api', f"repos/{REPOSITORY}/releases/{selection['id']}"))
    for asset in assets:
        found = next((item for item in uploaded['assets'] if item['name'] == asset.name), None)
        if not found or found.get('size') != asset.stat().st_size or found.get('digest') != 'sha256:' + sha(asset):
            raise ReleaseError('Uploaded asset verification failed: ' + asset.name)
    write_json(output / 'complete.json', {'release': selection, 'published': True,
                                         'assets': {p.name: sha(p) for p in assets}})
    runner.write('Published verified assets to ' + selection['url'])
