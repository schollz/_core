#!/usr/bin/env python3
from pathlib import Path
import argparse,os,subprocess,tempfile
from fixtures import create
root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--sdk',type=Path,default=root/'artifacts/Rack-SDK');a=p.parse_args()
sdk=a.sdk.resolve();out=root/'artifacts/rack-test';out.mkdir(exist_ok=True,parents=True)
subprocess.run(['make','-C',str(root/'rack'),'-j4',f'RACK_DIR={sdk}'],check=True)
cmd=[os.environ.get('CXX','clang++'),'-std=c++17','-O2','-g','-pthread',f'-I{sdk}/include',f'-I{sdk}/dep/include',str(root/'test/rack/module.cpp'),str(root/'rack/src/Storage.cpp')]
cmd += [str(root/'rack/build'/s) for s in ['core-engine.o','card-format.o','pcg.o']]
cmd += [f'-L{sdk}','-lRack','-o',str(out/'module-test')]
subprocess.run(cmd,check=True)
with tempfile.TemporaryDirectory() as folder:
    create(folder)
    env=dict(os.environ,DYLD_LIBRARY_PATH=str(sdk),LD_LIBRARY_PATH=str(sdk),PATH=str(sdk)+os.pathsep+os.environ['PATH'])
    subprocess.run([str(out/'module-test'),folder],check=True,env=env,timeout=120)
