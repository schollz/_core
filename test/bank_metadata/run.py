from pathlib import Path
import os
import subprocess
import tempfile
root = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory() as directory:
    exe = str(Path(directory) / 'metadata')
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-Itest/bank_metadata', '-Ilib',
                    'test/bank_metadata/test_metadata.c', '-lm', '-o', exe], cwd=root, check=True)
    subprocess.run([exe], cwd=root, check=True)
