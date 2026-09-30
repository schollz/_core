"""Native release orchestration; no application tests or runtime launches."""
from __future__ import annotations

import argparse
import os
import platform
import re
import shlex
import subprocess
from pathlib import Path

from package import package_build
from release_common import (REPOSITORY_URL, ROOT, SSH, ReleaseError, Runner,
                            clone_release, finish_package, new_run, publish,
                            require_tools, select_release, write_json)

DEFAULT_REMOTE = 'zns@192.168.0.44'
DEFAULT_IDENTITY = 'Developer ID Application: Zackary Scholl (KF253X8W3N)'
REMOTE_DIRECTORY = re.compile(r'/tmp/core-sample-manager-intel\.[A-Za-z0-9]+')


def build_commands(project, build, version, platform_name, jobs):
    configure = ['cmake', '-S', str(project), '-B', str(build), '-G', 'Ninja',
                 '-DCMAKE_BUILD_TYPE=Release', '-DCORE_MANAGER_VERSION=' + version]
    if platform_name.startswith('macos'):
        configure += ['-DCMAKE_OSX_ARCHITECTURES=' + platform_name.removeprefix('macos-'),
                      '-DCMAKE_OSX_DEPLOYMENT_TARGET=11.0']
    return [configure, ['cmake', '--build', str(build), '--config', 'Release',
                        '--parallel', str(jobs), '--target', 'CoreSampleManager']]


def mac_credentials(runner, profile):
    identity = os.environ.get('SIGN_IDENTITY', DEFAULT_IDENTITY)
    if not identity.startswith('Developer ID Application:'):
        raise ReleaseError('SIGN_IDENTITY must name a Developer ID Application identity')
    if identity not in runner.run('security', 'find-identity', '-v', '-p', 'codesigning'):
        raise ReleaseError('Developer ID Application certificate/private key is unavailable in this Mac’s Keychain')
    if profile:
        return identity, ['--keychain-profile', profile]
    missing = [key for key in ('APPLE_ID', 'TEAM_ID', 'APPLE_PASSWORD') if not os.environ.get(key)]
    if missing:
        raise ReleaseError('Set ' + ', '.join(missing) + ', or supply --notary-profile / NOTARY_PROFILE')
    return identity, ['--apple-id', os.environ['APPLE_ID'], '--team-id', os.environ['TEAM_ID'],
                      '--password', os.environ['APPLE_PASSWORD']]


def remote_build_script(directory, selection, jobs):
    """Clone independently on the Intel Mac and require the pinned local commit."""
    source = Path(directory) / 'source'
    project = source / 'sample-manager'
    build = project / 'build/macos-x86_64'
    quote = shlex.quote
    commands = build_commands(project, build, selection['version'], 'macos-x86_64', jobs)
    build_script = 'set -euo pipefail\n' + '\n'.join(shlex.join(c) for c in commands)
    return '\n'.join([
        'export PATH=/usr/local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin',
        'test "$(uname -s)" = Darwin && test "$(uname -m)" = x86_64',
        'for tool in git cmake ninja xcrun ditto; do command -v "$tool" >/dev/null; done',
        shlex.join(['git', 'clone', '--depth', '1', '--no-checkout', '--branch', selection['tag'],
                    REPOSITORY_URL, str(source)]),
        'test "$(git -C ' + quote(str(source)) + ' rev-parse HEAD)" = ' + quote(selection['commit']),
        shlex.join(['git', '-C', str(source), 'checkout', '--detach', selection['commit']]),
        'test -f ' + quote(str(project / 'CMakeLists.txt')),
        '/usr/bin/caffeinate -i /bin/bash -c ' + quote(build_script),
        shlex.join(['ditto', '-c', '-k', '--sequesterRsrc', '--keepParent',
                    str(build / 'CoreSampleManager_artefacts/Release/Core Sample Manager.app'),
                    directory + '/app.zip']),
        shlex.join(['cp', str(build / 'CMakeCache.txt'), directory + '/CMakeCache.txt']),
        shlex.join(['cp', str(project / '.cache/deps/juce-src/LICENSE.md'), directory + '/JUCE-LICENSE.md']),
    ])


def retrieve_intel(runner, host, directory, output, build):
    transport = ['rsync', '-cz', '--no-times', '-e', shlex.join(SSH)]
    for name in ('app.zip', 'CMakeCache.txt', 'JUCE-LICENSE.md'):
        runner.run(*transport, f'{host}:{directory}/{name}', output / name)
    artefacts = build / 'CoreSampleManager_artefacts/Release'
    artefacts.mkdir(parents=True)
    runner.run('ditto', '-x', '-k', output / 'app.zip', artefacts)
    # Only metadata is needed here; the compiler and cache paths remain remote.
    (build / 'CMakeCache.txt').write_bytes((output / 'CMakeCache.txt').read_bytes())
    return output / 'JUCE-LICENSE.md'


def main(platform_name):
    parser = argparse.ArgumentParser(description='Clone, build and publish the latest stable Core Sample Manager release.')
    remote = platform_name == 'macos-x86_64'
    mac = platform_name.startswith('macos')
    if remote:
        parser.add_argument('host', nargs='?', default=DEFAULT_REMOTE, help='Intel Mac SSH destination')
        parser.add_argument('--keep-remote', action='store_true', help='Keep the successful remote clone/build')
    parser.add_argument('--jobs', type=int, default=6)
    parser.add_argument('--output', type=Path, default=ROOT / 'dist/releases', help='Parent for a unique run directory')
    parser.add_argument('--no-upload', action='store_true', help='Build the latest tag and sign/package normally, retaining assets locally')
    if mac:
        parser.add_argument('--notary-profile', default=os.environ.get('NOTARY_PROFILE', ''), help='Existing local Keychain profile')
    args = parser.parse_args()
    runner = Runner()
    output = None
    remote_dir = None
    success = False
    try:
        if not 1 <= args.jobs <= 64:
            raise ReleaseError('--jobs must be between 1 and 64')
        if mac and platform.system() != 'Darwin':
            raise ReleaseError('Run macOS releases on the signing Mac')
        if platform_name == 'macos-arm64' and platform.machine() != 'arm64':
            raise ReleaseError('Use native ARM64 Python on Apple Silicon, outside Rosetta')
        if not mac and (platform.system() != 'Linux' or platform.machine() != 'x86_64'):
            raise ReleaseError('Run the Linux release directly on Linux x86_64')
        if remote and not re.fullmatch(r'(?:[A-Za-z0-9_.-]+@)?[A-Za-z0-9][A-Za-z0-9_.-]*', args.host):
            raise ReleaseError('Invalid Intel Mac SSH destination')
        require_tools('git', 'gh')
        if mac:
            require_tools('security', 'codesign', 'xcrun', 'xattr', 'lipo', 'otool', 'ditto', 'spctl')
        if remote:
            require_tools('ssh', 'rsync')
        else:
            require_tools('cmake', 'ninja')
        if not mac:
            require_tools('pkg-config', 'dpkg-query', 'ldconfig', 'ldd', 'getconf', 'readelf')
        output = new_run(args.output, platform_name)
        runner.log = output / 'release.log'
        selection = select_release(runner)
        write_json(output / 'selection.json', selection)
        project = clone_release(runner, selection, output / 'source')
        identity, credentials = mac_credentials(runner, args.notary_profile) if mac else (None, None)
        build = project / 'build' / platform_name
        juce_license = None
        if remote:
            remote_dir = runner.ssh(args.host, 'mktemp -d /tmp/core-sample-manager-intel.XXXXXX')
            if not REMOTE_DIRECTORY.fullmatch(remote_dir):
                raise ReleaseError('Unexpected remote temporary directory')
            runner.ssh(args.host, remote_build_script(remote_dir, selection, args.jobs))
            juce_license = retrieve_intel(runner, args.host, remote_dir, output, build)
        else:
            for command in build_commands(project, build, selection['version'], platform_name, args.jobs):
                runner.run(*(['/usr/bin/caffeinate', '-i'] if mac else []), *command)
        if not mac:
            header = runner.run('readelf', '-h', build / 'CoreSampleManager_artefacts/Release/Core Sample Manager')
            if not re.search(r'Class:\s+ELF64', header) or not re.search(r'Machine:\s+Advanced Micro Devices X86-64', header):
                raise ReleaseError('Expected a Linux x86_64 executable')
        assets = output / 'assets'
        archive, manifest = package_build(project, build, assets, platform_name, selection['version'],
            command=runner.run, sign_identity=identity, notary_credentials=credentials, juce_license=juce_license)
        manifest.update(buildHost=args.host if remote else platform.node(), signingHost=platform.node() if mac else None)
        finish_package(assets, archive, manifest, selection, platform_name, will_publish=not args.no_upload)
        # Keep payload and notarization diagnostics outside the upload directory.
        (assets / 'Core Sample Manager').rename(output / 'payload')
        if (assets / 'notarization.json').exists():
            (assets / 'notarization.json').rename(output / 'notarization.json')
        if args.no_upload:
            runner.write('Packaged locally (--no-upload); no publication completion marker: ' + str(assets))
        else:
            publish(runner, assets, platform_name)
        success = True
        return 0
    except (RuntimeError, OSError, ValueError, KeyError, subprocess.CalledProcessError) as error:
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
                    runner.write('Remote cleanup failed: ' + str(error))
            else:
                runner.write('Remote clone/build retained: ' + args.host + ':' + remote_dir)
