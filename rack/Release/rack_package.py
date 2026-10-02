"""Verify Rack's packaged binary and matching source, then make a sharing ZIP."""
from datetime import datetime, timezone
from pathlib import PurePosixPath
import hashlib
import io
import json
import re
import struct
import subprocess
import tarfile
import zipfile


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read_plugin(root):
    manifest = json.loads((root / 'rack/plugin.json').read_text())
    if not re.fullmatch(r'[A-Za-z0-9_-]+', manifest['slug']):
        raise ValueError('Invalid plugin slug')
    if not re.fullmatch(r'2\.[0-9]+\.[0-9]+(?:[-+][A-Za-z0-9.-]+)?', manifest['version']):
        raise ValueError('Expected a Rack 2 plugin version in plugin.json')
    return manifest


def verify_binary(data, platform_name):
    if platform_name.startswith('mac-'):
        cpu = 0x0100000c if platform_name == 'mac-arm64' else 0x01000007
        if (len(data) < 32 or data[:4] != b'\xcf\xfa\xed\xfe'
                or struct.unpack_from('<I', data, 4)[0] != cpu
                or struct.unpack_from('<I', data, 12)[0] != 6):
            raise ValueError('Packaged plugin is not a ' + platform_name + ' Mach-O dylib')
    elif (len(data) < 64 or data[:6] != b'\x7fELF\x02\x01'
          or struct.unpack_from('<HH', data, 16) != (3, 62)):
        raise ValueError('Packaged plugin is not a Linux x86_64 ELF shared library')


def verify_source(root, source, slug, version):
    prefix = f'{slug}-{version}-source/'
    with tarfile.open(source) as archive:
        hashes = json.load(archive.extractfile(prefix + 'SOURCE_SHA256.json'))
        required = {'rack/plugin.json', 'rack/Makefile', 'lib/core_engine/prepare.py',
                    'lib/crossfade4.py', 'lib/fuzz.py', 'lib/resonantfilter.py', 'main.c'}
        if not required.issubset(hashes):
            raise ValueError('Source archive lacks required build inputs')
        for path, expected in hashes.items():
            relative = PurePosixPath(path)
            if relative.is_absolute() or '..' in relative.parts:
                raise ValueError('Invalid source archive path: ' + path)
            member = archive.getmember(prefix + path)
            if not member.isfile():
                raise ValueError('Expected regular source file: ' + path)
            archived = archive.extractfile(member).read()
            if hashlib.sha256(archived).hexdigest() != expected or sha(root / path) != expected:
                raise ValueError('Source archive mismatch: ' + path)


def package(root, dist, output, platform_name, selection, runner, build_host):
    manifest = read_plugin(root)
    slug, version = manifest['slug'], manifest['version']
    name = f'{slug}-{version}-{platform_name}'
    plugin = dist / (name + '.vcvplugin')
    source = dist / f'{slug}-{version}-source.tar.gz'
    binary_name = 'plugin.dylib' if platform_name.startswith('mac-') else 'plugin.so'
    # Read the actual compressed plugin without extracting archive-supplied paths.
    data = subprocess.check_output(['zstd', '-dc', str(plugin)])
    with tarfile.open(fileobj=io.BytesIO(data)) as archive:
        packaged = json.load(archive.extractfile(f'{slug}/plugin.json'))
        if packaged != manifest:
            raise ValueError('Packaged plugin manifest does not match the source')
        binary = archive.extractfile(f'{slug}/{binary_name}').read()
        verify_binary(binary, platform_name)
        for entry in ['README.md', 'LICENSE.txt', 'THIRD_PARTY.md', 'res', 'licenses']:
            local = root / 'rack' / entry
            files = local.rglob('*') if local.is_dir() else [local]
            for file in files:
                if file.is_file():
                    member = f'{slug}/{file.relative_to(root / "rack").as_posix()}'
                    if archive.extractfile(member).read() != file.read_bytes():
                        raise ValueError('Packaged resource mismatch: ' + member)
    verify_source(root, source, slug, version)
    output.mkdir(parents=True, exist_ok=True)
    verification = output.parent / 'verification'
    verification.mkdir(exist_ok=True)
    payload = verification / binary_name
    payload.write_bytes(binary)
    payload.chmod(0o755)
    if platform_name.startswith('mac-'):
        runner.run('codesign', '--verify', '--strict', '--verbose=2', payload)
        dependencies = runner.run('otool', '-L', payload)
        if '/tmp/Rack2/libRack.dylib' not in dependencies:
            raise ValueError('Packaged plugin lacks the Rack runtime library path')
        signing = 'Rack SDK ad-hoc; no notarization'
    else:
        runner.run('readelf', '-h', payload)
        signing = 'unsigned'
    location = ('~/Library/Application Support/Rack2/plugins-' + platform_name + '/'
                if platform_name.startswith('mac-') else
                '$XDG_DATA_HOME/Rack2/plugins-lin-x64/ (default ~/.local/share/Rack2/plugins-lin-x64/)')
    instructions = f'''{manifest['name']} {version} - EZEPTOCORE for VCV Rack 2
Platform: {platform_name}; requires VCV Rack 2.6.6 or a later compatible Rack 2 release.

1. Extract this ZIP.
2. In Rack, choose Help > Open user folder, then quit Rack.
3. Copy {plugin.name} into plugins-{platform_name} inside that user folder.
   Create the directory if needed. Leave the .vcvplugin file packed.
   Default location: {location}
4. Restart Rack and add Infinite Digits > EZEPTOCORE.
5. Right-click the module, choose "Choose sample folder...", and select a
   completed sample-manager export containing bank1, bank2, and so on.
   Connect L OUT and R OUT to an audio output module.

Choose "Ectocore appearance" in the module menu for the alternate blue panel.
Samples are not included. Matching source and license notices are included.
Manual: {manifest['manualUrl']}
Source commit: {selection['source']['commit']}
'''
    build = {
        'plugin': slug, 'version': version, 'platform': platform_name,
        'commit': selection['source']['commit'], 'source': selection['source'],
        'release': selection['release'], 'build_host': build_host,
        'built_at_utc': datetime.now(timezone.utc).isoformat(),
        'signing': signing, 'runtime_tests_run': False,
    }
    files = {
        plugin.name: plugin.read_bytes(), source.name: source.read_bytes(),
        'INSTALL.txt': instructions.encode(),
        'BUILD.json': (json.dumps(build, indent=2) + '\n').encode(),
    }
    files['SHA256SUMS.txt'] = ''.join(
        f'{hashlib.sha256(content).hexdigest()}  {filename}\n'
        for filename, content in sorted(files.items())
    ).encode()
    output_zip = output / (name + '.zip')
    with zipfile.ZipFile(output_zip, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for filename, content in files.items():
            archive.writestr(f'{name}/{filename}', content)
    with zipfile.ZipFile(output_zip) as archive:
        if archive.testzip() is not None:
            raise ValueError('ZIP integrity check failed')
    checksums = output / (name + '-SHA256SUMS.txt')
    checksums.write_text(f'{sha(output_zip)}  {output_zip.name}\n', encoding='utf-8')
    return [output_zip, checksums]
