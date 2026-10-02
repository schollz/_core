#!/usr/bin/env python3
"""Bundle the Windows Rack package with matching source and install instructions."""
from datetime import datetime, timezone
from pathlib import Path
import hashlib
import io
import json
import os
import struct
import subprocess
import tarfile
import zipfile


def main():
    root = Path(__file__).resolve().parents[2]
    dist = root / 'rack/dist'
    manifest = json.loads((root / 'rack/plugin.json').read_text())
    slug, version = manifest['slug'], manifest['version']
    name = f'{slug}-{version}-win-x64'
    plugin = dist / (name + '.vcvplugin')
    source = dist / f'{slug}-{version}-source.tar.gz'
    commit = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip()

    # Verify the actual packaged DLL and manifest, not an intermediate build.
    data = subprocess.check_output(['zstd', '-dc', str(plugin)])
    with tarfile.open(fileobj=io.BytesIO(data)) as archive:
        packaged = json.load(archive.extractfile(f'{slug}/plugin.json'))
        if packaged != manifest:
            raise SystemExit('Packaged plugin manifest does not match the source')
        dll = archive.extractfile(f'{slug}/plugin.dll').read()
        pe = struct.unpack_from('<I', dll, 0x3c)[0]
        if dll[:2] != b'MZ' or dll[pe:pe + 4] != b'PE\0\0' or struct.unpack_from('<H', dll, pe + 4)[0] != 0x8664:
            raise SystemExit('Packaged plugin is not a Windows x64 DLL')

    # The source archive must correspond to this checkout, including generators.
    with tarfile.open(source) as archive:
        prefix = f'{slug}-{version}-source/'
        hashes = json.load(archive.extractfile(prefix + 'SOURCE_SHA256.json'))
        for path, expected in hashes.items():
            archived = archive.extractfile(prefix + path).read()
            if hashlib.sha256(archived).hexdigest() != expected or hashlib.sha256((root / path).read_bytes()).hexdigest() != expected:
                raise SystemExit(f'Source archive mismatch: {path}')

    instructions = f'''{manifest['name']} {version} - EZEPTOCORE for VCV Rack 2
Windows x64; requires VCV Rack 2.6.6 or a later compatible Rack 2 release.

1. Extract this ZIP.
2. In Rack, choose Help > Open user folder, then quit Rack.
3. Copy {plugin.name} into plugins-win-x64 inside that user folder.
   Create the directory if needed. Leave the .vcvplugin file packed.
   Default location: %LOCALAPPDATA%\\Rack2\\plugins-win-x64\\
4. Restart Rack and add Infinite Digits > EZEPTOCORE.
5. Right-click the module, choose "Choose sample folder...", and select a
   completed sample-manager export containing bank1, bank2, and so on.
   Connect L OUT and R OUT to an audio output module.

Choose "Ectocore appearance" in the module menu for the alternate blue panel.
Samples are not included. The matching source and license notices are included.
Manual: {manifest['manualUrl']}
Source commit: {commit}
'''
    build = {
        'plugin': slug, 'version': version, 'platform': 'win-x64',
        'commit': commit, 'built_at_utc': datetime.now(timezone.utc).isoformat(),
        'workflow_run': os.environ.get('GITHUB_RUN_ID'),
    }
    files = {
        plugin.name: plugin.read_bytes(), source.name: source.read_bytes(),
        'INSTALL.txt': instructions.encode(),
        'BUILD.json': (json.dumps(build, indent=2) + '\n').encode(),
    }
    files['SHA256SUMS.txt'] = ''.join(
        f'{hashlib.sha256(data).hexdigest()}  {filename}\n'
        for filename, data in sorted(files.items())
    ).encode()
    output = dist / (name + '.zip')
    with zipfile.ZipFile(output, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for filename, data in files.items():
            archive.writestr(f'{name}/{filename}', data)
    with zipfile.ZipFile(output) as archive:
        if archive.testzip() is not None:
            raise SystemExit('ZIP integrity check failed')
    (dist / (name + '-SHA256SUMS.txt')).write_text(
        f'{hashlib.sha256(output.read_bytes()).hexdigest()}  {output.name}\n',
        encoding='utf-8', newline='\n')
    print(output)


if __name__ == '__main__':
    main()
