from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory() as directory:
    exe = str(Path(directory) / 'screen')
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g',
                    '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined',
                    '-DZEPTOCORE_SCREEN=1', '-Itest/screen', '-Itest/bank_metadata', '-Ilib',
                    'test/screen/test_screen.c', '-o', exe], cwd=root, check=True)
    out = root / 'artifacts/zeptocore-screen/raster.pbm'
    out.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run([exe, str(out)], cwd=root, check=True)
