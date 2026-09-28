from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory() as directory:
    exe = str(Path(directory) / 'pattern')
    subprocess.run(['cc', '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-Ilib',
                    'test/input_detection/test_pattern.c', '-o', exe], cwd=root, check=True)
    subprocess.run([exe], check=True)
