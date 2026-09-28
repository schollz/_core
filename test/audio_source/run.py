from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory() as directory:
    for name in ('resample', 'silent_switch'):
        exe = str(Path(directory) / name)
        subprocess.run(['cc', '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                        '-fsanitize=address,undefined', '-Ilib', f'test/audio_source/test_{name}.c',
                        '-o', exe], cwd=root, check=True)
        subprocess.run([exe], check=True)
