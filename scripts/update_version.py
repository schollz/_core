#!/usr/bin/env python3
"""Update current release references without touching independent versions."""
import argparse
import json
import re
import subprocess
from pathlib import Path

from release_target import version_parts

ROOT = Path(__file__).resolve().parents[1]
CURRENT_PATHS = (
    'README.md', '.github/ISSUE_TEMPLATE/ectocore-freezing.md',
    'core/src/server/server.go', 'core/src/server/static/index.html',
    'lib/midi_comm.h', 'lib/zeptocore.h', 'test/midi/test_midi.c',
)
# These are deliberately historical, synthetic release fixtures. Current-version
# assertions (such as test/midi/test_midi.c) belong in CURRENT_PATHS instead.
HISTORICAL_PATHS = {
    'sample-manager/Tests/test_release.py', 'test/rack/test_release.py',
    'test/release/test_release.py',
}
RACK_START = '<!-- rack-downloads:start -->'
RACK_END = '<!-- rack-downloads:end -->'
RACK_PLATFORMS = (
    ('macOS (Apple Silicon)', 'mac-arm64'), ('macOS (Intel)', 'mac-x64'),
    ('Windows (x64)', 'win-x64'), ('Linux (x86_64)', 'lin-x64'),
)


def rack_downloads(root, version):
    plugin = json.loads((root / 'rack/plugin.json').read_text())
    if not re.fullmatch(r'[A-Za-z0-9_-]+', plugin['slug']):
        raise ValueError('Invalid Rack plugin slug')
    if not re.fullmatch(r'2\.[0-9]+\.[0-9]+(?:[-+][A-Za-z0-9.-]+)?', plugin['version']):
        raise ValueError('Expected a Rack 2 plugin version')
    rows = [RACK_START, '| Platform | Download |', '| --- | --- |']
    for label, platform in RACK_PLATFORMS:
        filename = f"{plugin['slug']}-{plugin['version']}-{platform}.zip"
        url = f'https://github.com/schollz/_core/releases/download/v{version}/{filename}'
        rows.append(f"| {label} | [v{plugin['version']} ZIP]({url}) |")
    return '\n'.join([*rows, RACK_END])


def version_pattern(version):
    # Allow v/z prefixes and filename suffixes, but not other numeric versions.
    return re.compile(r'(?<![0-9.])' + re.escape(version) + r'(?![0-9]|\.[0-9])')


def planned_updates(root, version):
    old = (root / 'VERSION').read_text().strip()
    if version_parts(version) <= version_parts(old):
        raise ValueError('New version must be greater than the current version')
    pattern = version_pattern(old)
    updates = {'VERSION': version + '\n'}
    for path in CURRENT_PATHS:
        text = (root / path).read_text()
        if not pattern.search(text):
            raise ValueError(f'{path} has no current-version reference ({old})')
        updates[path] = pattern.sub(version, text)
    readme = updates['README.md']
    if readme.count(RACK_START) != 1 or readme.count(RACK_END) != 1:
        raise ValueError('README must contain one marked Rack downloads table')
    before, rest = readme.split(RACK_START)
    _, after = rest.split(RACK_END)
    updates['README.md'] = before + rack_downloads(root, version) + after
    tracked = subprocess.check_output(['git', 'ls-files', '-z'], cwd=root).decode().split('\0')
    missed = []
    for path in tracked:
        if not path or path in HISTORICAL_PATHS:
            continue
        try:
            data = (root / path).read_bytes()
            if b'\0' in data:
                continue
            text = updates.get(path, data.decode('utf-8'))
        except UnicodeDecodeError:
            continue
        if pattern.search(text):
            missed.append(path)
    if missed:
        raise ValueError('Unhandled old release version in: ' + ', '.join(missed))
    return updates


def update(root, version, check=False):
    updates = planned_updates(root, version)
    if not check:
        for path, text in updates.items():
            (root / path).write_text(text)
    return updates


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('version')
    parser.add_argument('--check', action='store_true', help='Validate and preview without changing files')
    args = parser.parse_args()
    try:
        updates = update(ROOT, args.version, args.check)
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        parser.exit(1, str(error) + '\n')
    print(('Would update' if args.check else 'Updated') + ': ' + ', '.join(updates))


if __name__ == '__main__':
    main()
