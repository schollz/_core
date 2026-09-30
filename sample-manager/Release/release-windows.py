#!/usr/bin/env python3
"""Windows workflow orchestration, plus publication from the Linux upload job."""
from __future__ import annotations

import argparse
import os
import platform
import subprocess
from pathlib import Path

from release_common import (ROOT, ReleaseError, Runner, clone_release, finish_package,
                            new_run, publish, read_json, require_tools, select_release, write_json)


def export_environment(values):
    with open(os.environ['GITHUB_ENV'], 'a', encoding='utf-8') as handle:
        for key, value in values.items():
            value = str(value)
            if '\n' in value or '\r' in value:
                raise ReleaseError('Invalid multiline workflow environment value')
            handle.write(key + '=' + value + '\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('operation', choices=('prepare', 'build', 'package', 'publish'))
    parser.add_argument('--assets', type=Path, help='Downloaded assets for the publish operation')
    args = parser.parse_args()
    runner = Runner()
    output = None
    try:
        if args.operation == 'publish':
            if args.assets is None:
                raise ReleaseError('publish requires --assets')
            output = args.assets.resolve()
            if not output.is_dir():
                raise ReleaseError('Downloaded assets directory is missing')
            runner.log = output / 'publish.log'
            require_tools('gh', 'git')
            publish(runner, output, 'windows-x64')
            return 0
        if platform.system() != 'Windows':
            raise ReleaseError('Build operations must run on Windows')
        if args.operation == 'prepare':
            require_tools('gh', 'git', 'powershell')
            output = new_run(Path(os.environ['RUNNER_TEMP']), 'core-sample-manager-windows')
            export_environment({'CORE_RELEASE_ROOT': output})
            runner.log = output / 'prepare.log'
            selection = select_release(runner)
            write_json(output / 'selection.json', selection)
            project = clone_release(runner, selection, output / 'source')
            build = project / 'build/windows-x64'
            export_environment({'CORE_RELEASE_SOURCE': project, 'CORE_RELEASE_BUILD': build,
                                'CORE_RELEASE_VERSION': selection['version'],
                                'CORE_RELEASE_SIGN_DIR': build / 'CoreSampleManager_artefacts/Release',
                                'DOTNET_INSTALL_DIR': Path(os.environ['RUNNER_TOOL_CACHE']) / 'core-sample-manager-dotnet'})
            return 0
        output = Path(os.environ['CORE_RELEASE_ROOT'])
        runner.log = output / (args.operation + '.log')
        selection = read_json(output / 'selection.json')
        project = output / 'source/sample-manager'
        build = project / 'build/windows-x64'
        if runner.run('git', '-C', output / 'source', 'rev-parse', 'HEAD') != selection['commit']:
            raise ReleaseError('Source checkout no longer matches the selected release')
        script = ROOT / 'Release' / (args.operation + '-windows.ps1')
        command = ['powershell', '-NoProfile', '-NonInteractive', '-ExecutionPolicy', 'Bypass',
                   '-File', script, '-SourceRoot', project, '-BuildDirectory', build,
                   '-Version', selection['version']]
        if args.operation == 'build':
            runner.run(*command, '-SkipTests')
        else:
            staged = output / 'package'
            runner.run(*command, '-RequireSignature', '-OutputDirectory', staged)
            manifest = read_json(staged / 'manifest.json')
            manifest['buildHost'] = platform.node()
            assets = output / 'assets'
            assets.mkdir()
            archive = staged / f"_core-sample-manager-{selection['version']}-windows-x64.zip"
            destination = assets / archive.name
            archive.rename(destination)
            finish_package(assets, destination, manifest, selection, 'windows-x64')
        return 0
    except (ReleaseError, OSError, ValueError, KeyError, subprocess.CalledProcessError) as error:
        runner.write('Release failed: ' + str(error))
        if output:
            runner.write('Output and logs retained: ' + str(output))
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
