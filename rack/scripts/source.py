#!/usr/bin/env python3
"""Package the exact working sources, including uncommitted changes, without Git mutations."""
from pathlib import Path
import hashlib,json,tarfile,io
root=Path(__file__).resolve().parents[2]
plugin=json.loads((root/'rack/plugin.json').read_text())
output=root/'rack/dist';output.mkdir(exist_ok=True)
name=f"{plugin['slug']}-{plugin['version']}-source"
files=[root/'LICENSE',root/'Makefile',root/'main.c',root/'.github/workflows/build-rack.yml',root/'.github/workflows/release-rack-windows.yml']
files+=list((root/'lib').glob('*.h'))+[root/'lib/pcg_basic.c']
files += [root/'lib'/name for name in ['crossfade4.py', 'fuzz.py', 'resonantfilter.py']]
for directory in ['lib/core_engine','rack','test/rack']:
    files += [p for p in (root/directory).rglob('*') if p.is_file() and not any(x in p.relative_to(root/directory).parts for x in ['build','dist','__pycache__']) and p.suffix not in ['.dylib','.dll','.so','.pyc']]
manifest={str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(set(files))}
temporary=output/(name+'.tar.gz.tmp')
with tarfile.open(temporary,'w:gz') as archive:
    for path in manifest:
        data=(root/path).read_bytes();info=tarfile.TarInfo(name+'/'+path);info.size=len(data);info.mode=0o644;archive.addfile(info,io.BytesIO(data))
    data=json.dumps(manifest,indent=2).encode();info=tarfile.TarInfo(name+'/SOURCE_SHA256.json');info.size=len(data);info.mode=0o644;archive.addfile(info,io.BytesIO(data))
temporary.replace(output/(name+'.tar.gz'))
print(output/(name+'.tar.gz'))
