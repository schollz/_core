#!/usr/bin/env python3
"""Local-only native packaging. No publication, firmware downloads, or flashing."""
from __future__ import annotations
import argparse
import hashlib
import json
import platform
import plistlib
import re
import shutil
import subprocess
import tarfile
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def product_name(source_root):
    # The source checkout may differ from the release tooling: use its name.
    match = re.search(r'\bPRODUCT_NAME\s+"([A-Za-z0-9_ -]+)"',
                      (source_root / 'CMakeLists.txt').read_text())
    if not match:
        raise RuntimeError('Cannot read the application PRODUCT_NAME from CMakeLists.txt')
    return match.group(1)

def run(*args):
    print('+', ' '.join(map(str, args)), flush=True)
    return subprocess.check_output(list(map(str,args)), text=True, stderr=subprocess.STDOUT).strip()

def sha(path):
    digest = hashlib.sha256()
    with path.open('rb') as source:
        for block in iter(lambda: source.read(1024*1024), b''): digest.update(block)
    return digest.hexdigest()

def notices(destination, source_root=ROOT, juce_license=None):
    destination.mkdir(parents=True, exist_ok=True)
    files = {'LICENSE':'Application-GPLv3.txt', 'Vendor/PROVENANCE.md':'PROVENANCE.md',
             'Vendor/rubberband/COPYING':'Rubber-Band-GPLv2.txt',
             'Vendor/soundtouch/COPYING.TXT':'SoundTouch-LGPLv2.1.txt',
             '.cache/deps/juce-src/LICENSE.md':'JUCE-LICENSE.md'}
    for folder in ('Resources/Fonts', 'Resources/Icons/Lucide'):
        for path in (source_root/folder).glob('*'):
            if path.suffix.lower() in ('.txt','.md'):
                target = Path(folder).name + '-README.md' if path.name == 'README.md' else path.name
                files[str(path.relative_to(source_root))] = target
    for source, target in files.items():
        origin = juce_license if target == 'JUCE-LICENSE.md' and juce_license else source_root/source
        shutil.copy2(origin, destination/target)

def linux_library_notice(source, name, destination, command=run):
    # Native Debian/Ubuntu packaging records the notice shipped by the library's
    # owning package. Refuse a distribution with unidentified bundled libraries.
    candidates = {str(source), str(Path(source).resolve())}
    for candidate in list(candidates):
        if candidate.startswith('/lib/'):
            candidates.add('/usr' + candidate)
        elif candidate.startswith('/usr/lib/'):
            candidates.add(candidate.removeprefix('/usr'))
    owner = None
    for candidate in sorted(candidates):
        try:
            owner = command('dpkg-query', '-S', candidate).splitlines()[0].split(': ')[0]
            break
        except subprocess.CalledProcessError:
            pass
    if owner is None:
        raise RuntimeError('Cannot identify a redistribution notice for ' + str(source))
    package = owner.split(':')[0]
    copyright_file = Path('/usr/share/doc') / package / 'copyright'
    if not copyright_file.is_file():
        raise RuntimeError('Missing library copyright file ' + str(copyright_file))
    destination.mkdir(parents=True, exist_ok=True)
    shutil.copy2(copyright_file, destination / (package + '-copyright.txt'))
    return {'library': name, 'package': owner, 'source': str(source)}

def dependencies_linux(executable, destination, notice_directory, command=run):
    # glibc and the loader belong to the target system. Everything else resolved
    # by ldd, plus JUCE's dlopen dependencies, travels with the application.
    system = re.compile(r'^(?:ld-linux|lib(?:c|m|dl|pthread|rt|resolv|util)\.so)')
    destination.mkdir()
    catalog = command('ldconfig','-p')
    queue = [executable]
    provenance = []
    for name in ('libasound.so.2','libX11.so.6','libXext.so.6','libXinerama.so.1',
                 'libXrandr.so.2','libXcursor.so.1','libXrender.so.1',
                 'libfreetype.so.6','libfontconfig.so.1','libcurl.so.4'):
        candidates = [line.split(' => ')[1].strip() for line in catalog.splitlines()
                      if line.strip().startswith(name+' ') and 'x86-64' in line]
        if not candidates: raise RuntimeError('Missing runtime library '+name)
        provenance.append(linux_library_notice(candidates[0], name, notice_directory, command))
        shutil.copy2(candidates[0],destination/name)
        queue.append(destination/name)
    scanned = set()
    while queue:
        current = queue.pop()
        if str(current) in scanned: continue
        scanned.add(str(current))
        output = command('ldd', current)
        if 'not found' in output: raise RuntimeError(output)
        for line in output.splitlines():
            match = re.match(r'\s*(\S+) => (/\S+)',line)
            if not match: continue
            name, source = match.groups()
            if system.match(name): continue
            target = destination/name
            if not target.exists():
                provenance.append(linux_library_notice(source, name, notice_directory, command))
                shutil.copy2(source,target)
            if str(target) not in scanned: queue.append(target)
    (notice_directory / "libraries.json").write_text(json.dumps(provenance, indent=2) + "\n")
    return sorted(path.name for path in destination.iterdir())

def package_build(source_root, build, out, platform_name, version, *, command=run,
                  sign_identity=None, notary_credentials=None, juce_license=None):
    """Package an explicit build without building, testing, publishing or marking completion."""
    run = command
    mac = platform_name.startswith('macos')
    if notary_credentials and not sign_identity:
        raise RuntimeError('Notarization requires a Developer ID signing identity')
    cache=(build/'CMakeCache.txt').read_text()
    if not re.search(r'^CORE_MANAGER_VERSION:STRING='+re.escape(version)+r'$',cache,re.M): raise RuntimeError('Build version does not match requested package')
    stamp=datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ')
    name=product_name(source_root)
    payload=out/name;payload.mkdir(parents=True)
    notices(payload/'Notices', source_root, juce_license)
    (payload/'README.txt').write_text(name+' '+version+'\n\nOpen the application, choose a local folder, and import samples. Edits save automatically.\nThe folder includes portable immutable sources under .core-manager; keep that folder when copying projects.\nVisualizer Device mode needs opt-in telemetry firmware. Local UF2 tools require an explicit action.\nOnline drum analysis is the only application network request and runs only when selected.\nNo plugins or external audio tools are required. Keep the Notices directory.\n',encoding='utf-8')
    source=build/'CoreSampleManager_artefacts/Release'
    manifest={'application':name,'version':version,'platform':platform_name,
              'builtOn':platform.platform(),'created':stamp,'publication':'disabled; local artifacts only',
              'juce':'9.0.3','juceArchiveSHA256':'a81e5508b8a0efa483917794ebeaff56aed3075730c405a947734413f40c1aba'}
    if mac:
        app=payload/(name+'.app');shutil.copytree(source/app.name,app,symlinks=True)
        exe=app/'Contents/MacOS'/name
        arch=run('lipo','-archs',exe)
        if arch!=platform_name.removeprefix('macos-'): raise RuntimeError('Unexpected Mach-O architecture '+arch)
        info=plistlib.loads((app/'Contents/Info.plist').read_bytes())
        if info['CFBundleShortVersionString']!=version: raise RuntimeError('Bundle version mismatch')
        build_version=run('xcrun','vtool','-show-build',exe)
        minimum=re.search(r'^\s*minos\s+(\d+(?:\.\d+)+)\s*$',build_version,re.M)
        deployment=re.search(r'^CMAKE_OSX_DEPLOYMENT_TARGET:[^=]+=(.+)$',cache,re.M)
        if not minimum or not deployment or minimum.group(1)!=deployment.group(1):
            raise RuntimeError('Mach-O deployment target does not match the configured macOS minimum')
        minimum=minimum.group(1)
        if info.get('LSMinimumSystemVersion',minimum)!=minimum:
            raise RuntimeError('Bundle minimum macOS does not match the executable')
        # JUCE omits this Launch Services field; derive it from the audited binary
        # before signing the copied bundle, never from the packaging host version.
        info['LSMinimumSystemVersion']=minimum
        (app/'Contents/Info.plist').write_bytes(plistlib.dumps(info))
        linked=run('otool','-L',exe)
        for line in linked.splitlines()[1:]:
            path=line.strip().split(' (')[0]
            if not path.startswith(('/System/Library/','/usr/lib/')): raise RuntimeError('Non-system dependency '+path)
        sign=['codesign','--force','--sign',sign_identity or '-']
        if sign_identity: sign+=['--options','runtime','--timestamp']
        run('xattr','-cr',app)
        run(*sign,app);run('codesign','--verify','--deep','--strict',app)
        details=run('codesign','--display','--verbose=4',app)
        if sign_identity and ('Authority='+sign_identity not in details or 'Timestamp=' not in details):
            raise RuntimeError('Unexpected signing identity or missing signing timestamp')
        manifest.update(architecture=arch,minimumMacOS=minimum,dependencies=linked.splitlines()[1:],
                        signing='Developer ID' if sign_identity else 'ad-hoc local',signatureDetails=details)
        if notary_credentials:
            request=out/'notary-request.zip';run('ditto','-c','-k','--keepParent',app,request)
            result=json.loads(run('xcrun','notarytool','submit',request,*notary_credentials,'--wait','--timeout','30m','--output-format','json'))
            (out/'notarization.json').write_text(json.dumps(result,indent=2)+'\n')
            if result.get('status')!='Accepted':
                if result.get('id'):
                    run('xcrun','notarytool','log',result['id'],*notary_credentials,out/'notarization-log.json')
                raise RuntimeError('Notarization was not accepted; see notarization.json and release.log')
            run('xcrun','stapler','staple',app);run('xcrun','stapler','validate',app)
            run('spctl','--assess','--type','execute',app)
            manifest['notarization']=result;request.unlink()
        archive=out/('_core-sample-manager-'+version+'-'+platform_name+'.zip')
        run('ditto','-c','-k','--sequesterRsrc','--keepParent',payload,archive)
    else:
        exe=payload/name;shutil.copy2(source/exe.name,exe);exe.chmod(0o755)
        manifest['bundledLibraries']=dependencies_linux(exe,payload/'lib',payload/'Notices/Linux-Libraries',run)
        manifest['glibc']=run('getconf','GNU_LIBC_VERSION')
        launcher=payload/'core-sample-manager'
        launcher.write_text('#!/bin/sh\nset -eu\nHERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)\nexport LD_LIBRARY_PATH="$HERE/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"\nexec "$HERE/'+name+'" "$@"\n')
        launcher.chmod(0o755)
        archive=out/('_core-sample-manager-'+version+'-'+platform_name+'.tar.gz')
        with tarfile.open(archive,'w:gz') as tar: tar.add(payload,arcname=payload.name)
    manifest['files']={str(p.relative_to(payload)):sha(p) for p in sorted(payload.rglob('*')) if p.is_file()}
    manifest['archiveSHA256']=sha(archive)
    return archive, manifest

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--platform', choices=['macos-arm64','macos-x86_64','linux-x86_64'],required=True)
    parser.add_argument('--version',default='0.1.0')
    parser.add_argument('--jobs',type=int,default=6)
    parser.add_argument('--skip-build',action='store_true',help='Package an already validated matching preset build')
    parser.add_argument('--sign-identity',help='Optional macOS Developer ID; absent means ad-hoc local signing')
    parser.add_argument('--notary-profile',help='Explicit opt-in notarization using an existing Keychain profile')
    args=parser.parse_args()
    if not re.fullmatch(r'(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)',args.version): parser.error('Use major.minor.patch')
    mac=args.platform.startswith('macos')
    if (mac and platform.system()!='Darwin') or (not mac and (platform.system()!='Linux' or platform.machine()!='x86_64')): parser.error('Run packaging on its native operating system')
    if args.notary_profile and not args.sign_identity: parser.error('Notarization requires --sign-identity')
    build=ROOT/'build'/args.platform
    if not args.skip_build:
        run('cmake','--preset',args.platform,'-S',ROOT,'-DCORE_MANAGER_VERSION='+args.version)
        run('cmake','--build',build,'--parallel',str(args.jobs))
        # An Intel cross-build is not a native-host runtime qualification.
        if not mac or platform.machine()==args.platform.removeprefix('macos-'):
            run('ctest','--test-dir',build,'--output-on-failure')
    stamp=datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ')
    out=ROOT/'dist'/(args.platform+'-'+args.version+'-'+stamp)
    credentials=['--keychain-profile',args.notary_profile] if args.notary_profile else None
    archive,manifest=package_build(ROOT,build,out,args.platform,args.version,
        sign_identity=args.sign_identity,notary_credentials=credentials)
    report=out/'manifest.json';report.write_text(json.dumps(manifest,indent=2)+'\n')
    (out/'SHA256SUMS.txt').write_text('\n'.join(sha(p)+'  '+p.name for p in (archive,report))+'\n')
    (out/'complete.json').write_text(json.dumps({'archive':archive.name,'manifest':report.name})+'\n')
    print(out)
if __name__=='__main__': main()
