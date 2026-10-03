from pathlib import Path
import os
import subprocess
import tempfile
root = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory() as directory:
    # Consume triggers through the production callback block, including the
    # granular reset. This guards against clearing a trigger before resetting.
    callback = (root / 'lib/audio_callback.h').read_text()
    start = callback.index('  if (phase_change) {')
    end = callback.index('\n  if (audio_was_muted)', start)
    (Path(directory) / 'consume_phase.h').write_text(callback[start:end])
    for name, frames in (('resample',441), ('silent_switch',441), ('stretch',256), ('stretch',441)):
        exe = str(Path(directory) / f"{name}-{frames}")
        subprocess.run([os.environ.get('CC','cc'), '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                        '-fsanitize=address,undefined', '-Ilib', f'-I{directory}', f'-DSAMPLES_PER_BUFFER={frames}', f'test/audio_source/test_{name}.c',
                        '-lm', '-o', exe], cwd=root, check=True)
        subprocess.run([exe], check=True, timeout=45)
