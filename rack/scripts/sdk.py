#!/usr/bin/env python3
"""Download the pinned official SDK and verify it before extraction."""
from pathlib import Path
import argparse,hashlib,urllib.request,zipfile
VERSION='2.6.6'
SHA={
 'mac-arm64':'29414e52417992cbafa47e30f947c3c0c7a34e5c424bb83c5a0af8c24840481f',
 'mac-x64':'9b8b0d7582ca25fac879f8f64de40f2481df2fd903b65f3abb5d6801689060ec',
 'lin-x64':'420da2452def7b195f98e3a0650c35f1e0f058a98a71840b4347d3d203618532',
 'win-x64':'12f2043311db98592bdf73a7797e8a957c47950c3c8c17c7ff6421eb0015a05c'}
p=argparse.ArgumentParser();p.add_argument('architecture',choices=SHA);p.add_argument('--output',type=Path,default=Path('artifacts/rack-sdk'));a=p.parse_args()
a.output.mkdir(parents=True,exist_ok=True);archive=a.output/f'Rack-SDK-{VERSION}-{a.architecture}.zip'
if not archive.exists():urllib.request.urlretrieve('https://vcvrack.com/downloads/'+archive.name,archive)
if hashlib.sha256(archive.read_bytes()).hexdigest()!=SHA[a.architecture]:raise SystemExit('SDK checksum mismatch: remove the download and retry')
with zipfile.ZipFile(archive) as z:z.extractall(a.output)
print((a.output/'Rack-SDK').resolve())
