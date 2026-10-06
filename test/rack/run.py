#!/usr/bin/env python3
"""Generate from shared firmware and exercise the standalone engine."""
from pathlib import Path
import argparse, os, subprocess, sys
root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--sanitize',choices=['undefined','address,undefined'])
p.add_argument('--source-root',type=Path,default=root,help='Source checkout to generate and test')
a=p.parse_args();root=a.source_root.resolve()
out=root/'artifacts/rack-test';out.mkdir(parents=True,exist_ok=True)
clang=os.environ.get('CLANG','clang')
subprocess.run([sys.executable,str(root/'lib/core_engine/prepare.py'),'--out',str(out),'--clang',clang],check=True)
flags=['-std=c11','-O2','-g','-fwrapv','-ffp-contract=off','-Wno-everything','-pthread']
if a.sanitize:flags+=['-fsanitize='+a.sanitize,'-fno-sanitize-recover=all','-fno-sanitize=shift-base']
subprocess.run([clang,*flags,'-I'+str(root/'lib'),str(root/'test/rack/clock_input.c'),'-lm','-o',str(out/'clock-input-test')],check=True)
subprocess.run([str(out/'clock-input-test')],check=True,timeout=45)
subprocess.run([clang,*flags,'-c',str(out/'engine.c'),'-o',str(out/'engine.o')],check=True)
for name in ['engine','clock','tempo']:
    subprocess.run([clang,*flags,'-I'+str(root/'lib/core_engine'),str(out/'engine.o'),str(root/'lib/pcg_basic.c'),str(root/f'test/rack/{name}.c'),'-lm','-o',str(out/f'{name}-test')],check=True)
    # The tempo matrix simulates long low-BPM chains, including under ASan.
    subprocess.run([str(out/f'{name}-test')],check=True,timeout=120 if name=='tempo' else 45)
# The engine already contains preprocessed system declarations. Expand only the
# test's header macros first, then append its body without including libc twice.
# This retains private scheduler access and the platform's normal assertions.
headers=out/'variable-splice-headers.h'
headers.write_text('#include <assert.h>\n#include <stdbool.h>\n#include <stdio.h>\n')
(out/'empty-engine.h').write_text('')
test_body=subprocess.check_output([clang,*flags,'-E','-P','-imacros',str(headers),
    '-DVARIABLE_ENGINE_SOURCE="empty-engine.h"','-I'+str(out),
    str(root/'test/rack/variable_splice.c')],text=True)
variable_source=out/'variable-splice.c'
variable_source.write_text((out/'engine.c').read_text()+'\n'+test_body)
subprocess.run([clang,*flags,str(variable_source),str(root/'lib/pcg_basic.c'),'-lm','-o',str(out/'variable-splice-test')],check=True)
subprocess.run([str(out/'variable-splice-test')],check=True,timeout=45)
# Exercise the real asynchronous storage worker against generated card exports.
import tempfile, hashlib
sys.path.insert(0,str(root/'test/rack'))
from fixtures import create
cppflags=['-std=c++17','-O2','-g','-pthread']
if a.sanitize:cppflags+=['-fsanitize='+a.sanitize,'-fno-sanitize-recover=all']
subprocess.run([os.environ.get('CC','clang'),*flags,'-c',str(root/'lib/core_engine/card_format.c'),'-o',str(out/'card.o')],check=True)
subprocess.run([os.environ.get('CXX','clang++'),*cppflags,str(root/'rack/src/Storage.cpp'),str(root/'test/rack/storage.cpp'),str(out/'card.o'),'-o',str(out/'storage-test')],check=True)
with tempfile.TemporaryDirectory() as folder:
    create(folder)
    def hashes():return {str(p.relative_to(folder)):hashlib.sha256(p.read_bytes()).hexdigest() for p in Path(folder).rglob('*') if p.is_file()}
    before=hashes()
    subprocess.run([str(out/'storage-test'),folder],check=True,timeout=45)
    assert before==hashes(),'The source folder was modified'
    print('storage: source folder hashes unchanged')
