#!/usr/bin/env python3
"""Generate from shared firmware and exercise the standalone engine."""
from pathlib import Path
import argparse, os, subprocess, sys
root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--sanitize',choices=['undefined','address,undefined']);a=p.parse_args()
out=root/'artifacts/rack-test';out.mkdir(parents=True,exist_ok=True)
clang=os.environ.get('CLANG','clang')
subprocess.run([sys.executable,str(root/'lib/core_engine/prepare.py'),'--out',str(out),'--clang',clang],check=True)
flags=['-std=c11','-O2','-g','-fwrapv','-ffp-contract=off','-Wno-everything','-pthread']
if a.sanitize:flags+=['-fsanitize='+a.sanitize,'-fno-sanitize-recover=all','-fno-sanitize=shift-base']
subprocess.run([clang,*flags,'-I'+str(root/'lib/core_engine'),str(out/'engine.c'),str(root/'lib/pcg_basic.c'),str(root/'test/rack/engine.c'),'-lm','-o',str(out/'engine-test')],check=True)
subprocess.run([str(out/'engine-test')],check=True,timeout=45)
# Exercise the real asynchronous storage worker against generated card exports.
import tempfile, hashlib
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
