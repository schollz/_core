#!/usr/bin/env python3
"""Verify the root package and rebuild its matching source archive in isolation."""
import argparse
import io
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tarfile
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'rack/Release'))
import rack_package


def verify_package(root, architecture):
    manifest = json.loads((root / 'plugin.json').read_bytes())
    slug, version = manifest['slug'], manifest['version']
    name = f'{slug}-{version}-{architecture}.vcvplugin'
    package = root / 'dist' / name
    if package.read_bytes() != (root / 'rack/dist' / name).read_bytes():
        raise ValueError('Root package differs from Rack output')
    data = subprocess.check_output(['zstd', '-dc', str(package)])
    with tarfile.open(fileobj=io.BytesIO(data)) as archive:
        if archive.extractfile(f'{slug}/plugin.json').read() != (root / 'plugin.json').read_bytes():
            raise ValueError('Packaged manifest differs from the root manifest')
        extension = 'dylib' if architecture.startswith('mac-') else ('dll' if architecture == 'win-x64' else 'so')
        binary = archive.extractfile(f'{slug}/plugin.{extension}').read()
        if architecture == 'win-x64':
            pe = struct.unpack_from('<I', binary, 0x3c)[0]
            if (binary[:2] != b'MZ' or binary[pe:pe + 4] != b'PE\0\0'
                    or struct.unpack_from('<H', binary, pe + 4)[0] != 0x8664):
                raise ValueError('Expected a Windows x64 DLL')
        else:
            rack_package.verify_binary(binary, architecture)
        for entry in ('res', 'licenses', 'README.md', 'LICENSE.txt', 'THIRD_PARTY.md'):
            local = root / 'rack' / entry
            for path in (local.rglob('*') if local.is_dir() else [local]):
                if path.is_file():
                    member = f'{slug}/{path.relative_to(root / "rack").as_posix()}'
                    if archive.extractfile(member).read() != path.read_bytes():
                        raise ValueError('Packaged resource mismatch: ' + member)
    print('Verified root package:', name, flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk', required=True, type=Path)
    parser.add_argument('--arch', required=True, choices=('mac-arm64', 'mac-x64', 'lin-x64', 'win-x64'))
    args = parser.parse_args()
    sdk = args.sdk.resolve().as_posix()
    manifest = rack_package.read_plugin(ROOT)
    slug, version = manifest['slug'], manifest['version']
    source = ROOT / 'rack/dist' / f'{slug}-{version}-source.tar.gz'
    rack_package.verify_source(ROOT, source, slug, version)
    verify_package(ROOT, args.arch)
    with tempfile.TemporaryDirectory(prefix='rack-source-') as directory:
        with tarfile.open(source) as archive:
            archive.extractall(directory, filter='data')
        extracted = Path(directory) / f'{slug}-{version}-source'
        for goal in ('clean', 'cleandep', 'dep', 'dist'):
            python = 'python' if os.name == 'nt' else sys.executable
            subprocess.run(['make', f'RACK_DIR={sdk}', f'PYTHON={python}',
                            '-j4', goal], cwd=extracted, check=True)
        verify_package(extracted, args.arch)
    print('Source archive rebuilt successfully without the original checkout', flush=True)


if __name__ == '__main__':
    main()
