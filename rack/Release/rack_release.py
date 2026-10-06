"""Fresh-main native Rack builds and verified publication to an existing release."""
from __future__ import annotations

import argparse
import json
import os
import platform
import re
import shlex
import shutil
import subprocess
import sys
import tarfile
import tempfile
from pathlib import Path

from rack_package import package, read_plugin, sha

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT.parent / 'scripts'))
from release_target import target_arguments, validate_target, verify_tag
REPOSITORY = 'schollz/_core'
REPOSITORY_URL = 'https://github.com/' + REPOSITORY + '.git'
DEFAULT_REMOTE = 'zns@192.168.0.44'
REMOTE_DIRECTORY = re.compile(r'/tmp/core-rack-intel\.[A-Za-z0-9]+')
SSH = ['ssh', '-o', 'BatchMode=yes', '-o', 'ForwardAgent=no', '-o', 'ConnectTimeout=10']
PRIVATE_ENV = ('APPLE_ID', 'TEAM_ID', 'APPLE_PASSWORD', 'SIGN_IDENTITY',
               'NOTARY_PROFILE', 'GH_TOKEN', 'GITHUB_TOKEN', 'AZURE_CLIENT_SECRET')


class Runner:
    def __init__(self):
        self.log = None
        self.secrets = [os.environ[key] for key in PRIVATE_ENV if os.environ.get(key)]

    def write(self, value):
        for secret in self.secrets:
            value = value.replace(secret, '<redacted>')
        print(value, flush=True)
        if self.log:
            with self.log.open('a', encoding='utf-8') as handle:
                handle.write(value + '\n')
        return value

    def run(self, *args, cwd=None):
        args = list(map(str, args))
        self.write('+ ' + shlex.join(args))
        environment = os.environ.copy()
        environment['LC_ALL'] = 'C'
        if args[0] != 'gh':
            for key in PRIVATE_ENV:
                environment.pop(key, None)
        # Release targets can be invoked through Make; don't inherit build overrides
        # or a parent jobserver into the independent fresh checkout.
        for key in ('MAKEFLAGS', 'MFLAGS', 'MAKEOVERRIDES', 'CROSS_COMPILE', 'ENGINE_TARGET', 'CODESIGN'):
            environment.pop(key, None)
        with subprocess.Popen(args, cwd=cwd, env=environment, stdout=subprocess.PIPE,
                              stderr=subprocess.STDOUT, text=True, encoding='utf-8',
                              errors='replace') as process:
            lines = [self.write(line.rstrip('\n')) for line in process.stdout]
            code = process.wait()
        output = '\n'.join(lines).strip()
        if code:
            raise subprocess.CalledProcessError(code, args, output=output)
        return output

    def ssh(self, host, script):
        return self.run(*SSH, host, '/bin/bash -c ' + shlex.quote('set -euo pipefail\n' + script))


def require_tools(*names):
    missing = [name for name in names if not shutil.which(name)]
    if missing:
        raise RuntimeError('Install required tools first: ' + ', '.join(missing))


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8')


def latest_release(runner, tag=""):
    endpoint = 'tags/' + tag if tag else 'latest'
    release = json.loads(runner.run('gh', 'api', f'repos/{REPOSITORY}/releases/{endpoint}',
        '--jq', '{id, tag_name, html_url, draft, prerelease, immutable}'))
    if (not release.get('id') or not release.get('tag_name') or release.get('draft')
            or release.get('prerelease') or release.get('immutable')):
        raise RuntimeError('Latest release must be published, stable and mutable')
    return {'repository': REPOSITORY, 'id': release['id'],
            'tag': release['tag_name'], 'url': release['html_url']}


def select_release(runner, release_tag='', source_commit=''):
    validate_target(release_tag, source_commit)
    if release_tag:
        verify_tag(runner, REPOSITORY_URL, release_tag, source_commit)
        release = latest_release(runner, release_tag)
        if release['tag'] != release_tag:
            raise ValueError('Named release does not match the requested tag')
        return {'release': release, 'source': {'repository': REPOSITORY,
                'ref': 'refs/tags/' + release_tag, 'commit': source_commit}}
    release = latest_release(runner)
    ref = 'refs/heads/main'
    output = runner.run('git', 'ls-remote', '--exit-code', '--heads', REPOSITORY_URL, ref)
    refs = dict(line.split()[::-1] for line in output.splitlines() if len(line.split()) == 2)
    commit = refs.get(ref, '')
    if not re.fullmatch(r'[0-9a-f]{40}', commit):
        raise RuntimeError('Cannot resolve main to a commit')
    return {'release': release, 'source': {'repository': REPOSITORY, 'ref': ref, 'commit': commit}}


def clone_commands(selection, source):
    commit = selection['source']['commit']
    return [
        ['git', 'clone', '--depth', '1', '--no-checkout', '--branch', selection['source']['ref'].removeprefix('refs/heads/').removeprefix('refs/tags/'), REPOSITORY_URL, str(source)],
        ['git', '-C', str(source), 'fetch', '--depth', '1', 'origin', commit],
        ['git', '-C', str(source), 'checkout', '--detach', commit],
    ]


def clone_source(runner, selection, source):
    for command in clone_commands(selection, source):
        runner.run(*command)
    if runner.run('git', '-C', source, 'rev-parse', 'HEAD') != selection['source']['commit']:
        raise RuntimeError('Source checkout does not match the selected main commit')
    read_plugin(source)


def build_commands(source, platform_name, jobs, python):
    compiler = ['CC=clang', 'CXX=clang++'] if platform_name.startswith('mac-') else ['CC=gcc', 'CXX=g++']
    return [
        [python, '-c', 'import numpy'],
        [python, str(source / 'rack/scripts/sdk.py'), platform_name,
         '--output', str(source / 'artifacts/rack-sdk')],
        ['make', '-C', str(source / 'rack'), 'RACK_DIR=' + str(source / 'artifacts/rack-sdk/Rack-SDK'),
         'PYTHON=' + python, 'CLANG=clang', *compiler, '-j' + str(jobs), 'dist'],
        [python, str(source / 'rack/scripts/source.py')],
    ]


def prepare_intel_inputs(runner, source):
    """Prepare portable inputs here; preprocess libc and compile on the Intel Mac."""
    runner.run(sys.executable, '-c', 'import numpy')
    sdk = source / 'artifacts/rack-sdk'
    runner.run(sys.executable, source / 'rack/scripts/sdk.py', 'mac-x64', '--output', sdk)
    generated = source / 'rack/build/generated'
    runner.run(sys.executable, source / 'lib/core_engine/prepare.py',
               '--out', generated, '--source-only')
    archives = list(sdk.glob('Rack-SDK-*-mac-x64.zip'))
    if len(archives) != 1:
        raise RuntimeError('Expected one verified Intel Rack SDK archive')
    return [archives[0], generated / 'firmware.c']


def remote_build_script(directory, selection, jobs, manifest):
    source = Path(directory) / 'source'
    generated = source / 'rack/build/generated'
    commands = [
        ['python3', str(source / 'rack/scripts/sdk.py'), 'mac-x64', '--output', directory],
        ['mkdir', '-p', str(generated)],
        ['cp', str(Path(directory) / 'firmware.c'), str(generated / 'firmware.c')],
        ['python3', str(source / 'lib/core_engine/prepare.py'), '--out', str(generated),
         '--clang', 'clang', '--generate-only'],
        # SDK manifest variables override its jq calls. The engine has already
        # been generated with native headers; Make must not regenerate its tables.
        ['make', '-C', str(source / 'rack'), 'RACK_DIR=' + str(Path(directory) / 'Rack-SDK'),
         'PYTHON=python3', 'CLANG=clang', 'CC=clang', 'CXX=clang++',
         'SLUG=' + manifest['slug'], 'VERSION=' + manifest['version'],
         '--assume-old=build/generated/engine.c', '-j' + str(jobs), 'dist'],
        ['python3', str(source / 'rack/scripts/source.py')],
    ]
    build = 'set -euo pipefail\n' + '\n'.join(shlex.join(command) for command in commands)
    return '\n'.join([
        'export PATH=/usr/local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin',
        'test "$(uname -s)" = Darwin && test "$(uname -m)" = x86_64',
        'for tool in git python3 make clang clang++ zstd rsync codesign install_name_tool otool; do command -v "$tool" >/dev/null || { echo "Missing Intel build tool: $tool" >&2; exit 1; }; done',
        'python3 -c ' + shlex.quote('import sys, platform; assert sys.version_info >= (3, 8), "Intel build requires Python 3.8+"; assert platform.machine() == "x86_64"'),
        *(shlex.join(command) for command in clone_commands(selection, source)),
        'test "$(git -C ' + shlex.quote(str(source)) + ' rev-parse HEAD)" = ' + shlex.quote(selection['source']['commit']),
        'cd ' + shlex.quote(str(source)),
        '/usr/bin/caffeinate -i /bin/bash -c ' + shlex.quote(build),
    ])


def publish(runner, selection, assets):
    release = selection['release']
    pinned = selection['source']['ref'].startswith('refs/tags/')
    if pinned:
        verify_tag(runner, REPOSITORY_URL, release['tag'], selection['source']['commit'])
    if latest_release(runner, release['tag'] if pinned else '') != release:
        raise RuntimeError(('Selected' if pinned else 'Latest') + ' release changed during the build; upload aborted, artifacts retained')
    hashes = {asset.name: sha(asset) for asset in assets}
    runner.run('gh', 'release', 'upload', '--repo', REPOSITORY, '--clobber', '--', release['tag'], *assets)
    verified = assets[0].parent.parent / 'published-verify'
    verified.mkdir()
    for asset in assets:
        runner.run('gh', 'release', 'download', '--repo', REPOSITORY, '--dir', verified,
                   '--pattern', asset.name, '--', release['tag'])
        if sha(verified / asset.name) != hashes[asset.name]:
            raise RuntimeError('Uploaded asset verification failed: ' + asset.name)
    write_json(assets[0].parent / 'complete.json', {**selection, 'published': True, 'assets': hashes})
    runner.write('Published and downloaded verified assets: ' + release['url'])


def main(platform_name, argv=None):
    parser = argparse.ArgumentParser(description='Build and upload Rack; default to main/latest, or pin a release tag and source commit.')
    remote = platform_name == 'mac-x64'
    if remote:
        parser.add_argument('host', nargs='?', default=DEFAULT_REMOTE)
        parser.add_argument('--keep-remote', action='store_true')
    parser.add_argument('--jobs', type=int, default=6)
    parser.add_argument('--output', type=Path, default=ROOT / 'dist/releases')
    parser.add_argument('--no-upload', action='store_true')
    target_arguments(parser)
    args = parser.parse_args(argv)
    runner = Runner()
    output = remote_dir = None
    success = False
    try:
        if sys.version_info < (3, 10):
            raise RuntimeError('Python 3.10 or later is required')
        if not 1 <= args.jobs <= 64:
            raise RuntimeError('--jobs must be between 1 and 64')
        if platform_name.startswith('mac-') and platform.system() != 'Darwin':
            raise RuntimeError('Run macOS releases from a Mac')
        if platform_name == 'mac-arm64' and platform.machine() != 'arm64':
            raise RuntimeError('Use native ARM64 Python on Apple Silicon, outside Rosetta')
        if platform_name == 'lin-x64' and (platform.system(), platform.machine()) != ('Linux', 'x86_64'):
            raise RuntimeError('Run the Linux release directly on Linux x86_64')
        if remote and not re.fullmatch(r'(?:[A-Za-z0-9_.-]+@)?[A-Za-z0-9][A-Za-z0-9_.-]*', args.host):
            raise RuntimeError('Invalid Intel Mac SSH destination')
        require_tools('git', 'gh', 'zstd')
        if platform_name.startswith('mac-'):
            require_tools('codesign', 'otool')
        else:
            require_tools('readelf')
        if remote:
            require_tools('ssh', 'rsync')
        else:
            require_tools('make', 'clang', 'jq', *(['clang++', 'rsync', 'install_name_tool']
                          if platform_name.startswith('mac-') else ['gcc', 'g++', 'strip']))
        args.output.mkdir(parents=True, exist_ok=True)
        output = Path(tempfile.mkdtemp(prefix=platform_name + '-', dir=args.output.resolve()))
        runner.log = output / 'release.log'
        runner.write('Release output: ' + str(output))
        selection = select_release(runner, args.release_tag, args.source_commit)
        write_json(output / 'selection.json', selection)
        source = output / 'source'
        clone_source(runner, selection, source)
        manifest = read_plugin(source)
        if remote:
            inputs = prepare_intel_inputs(runner, source)
            remote_dir = runner.ssh(args.host, 'mktemp -d /tmp/core-rack-intel.XXXXXX')
            if not REMOTE_DIRECTORY.fullmatch(remote_dir):
                raise RuntimeError('Unexpected remote temporary directory')
            runner.run('rsync', '-cz', '--no-times', '-e', shlex.join(SSH),
                       *inputs, f'{args.host}:{remote_dir}/')
            runner.ssh(args.host, remote_build_script(remote_dir, selection, args.jobs, manifest))
            dist = output / 'remote-dist'
            dist.mkdir()
            slug, version = manifest['slug'], manifest['version']
            for name in (f'{slug}-{version}-mac-x64.vcvplugin', f'{slug}-{version}-source.tar.gz'):
                runner.run('rsync', '-cz', '--no-times', '-e', shlex.join(SSH),
                           f'{args.host}:{remote_dir}/source/rack/dist/{name}', dist / name)
        else:
            for command in build_commands(source, platform_name, args.jobs, sys.executable):
                runner.run(*(['/usr/bin/caffeinate', '-i'] if platform_name.startswith('mac-') else []),
                           *command, cwd=source)
            dist = source / 'rack/dist'
        assets = package(source, dist, output / 'assets', platform_name, selection, runner,
                         args.host if remote else platform.node())
        if args.no_upload:
            runner.write('Packaged locally (--no-upload): ' + str(output / 'assets'))
        else:
            publish(runner, selection, assets)
        success = True
        return 0
    except (RuntimeError, OSError, ValueError, KeyError, subprocess.CalledProcessError,
            EOFError, tarfile.TarError) as error:
        runner.write('Release failed: ' + str(error))
        if output:
            runner.write('Output and logs retained: ' + str(output))
        return 1
    finally:
        if remote_dir and REMOTE_DIRECTORY.fullmatch(remote_dir):
            if success and not args.keep_remote:
                try:
                    runner.ssh(args.host, 'rm -rf -- ' + shlex.quote(remote_dir))
                except (OSError, subprocess.CalledProcessError) as error:
                    runner.write('Remote cleanup failed; retained at ' + args.host + ':' + remote_dir + ': ' + str(error))
            else:
                runner.write('Remote clone/build retained: ' + args.host + ':' + remote_dir)
