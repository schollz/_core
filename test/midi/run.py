from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
configurations = {
    "zeptocore": ["-DINCLUDE_ZEPTOCORE=1", "-DEXPECT_NOTE_KEY=1"],
    "disabled": ["-DINCLUDE_ZEPTOCORE=1", "-DMIDI_NOTE_KEY=0", "-DEXPECT_NOTE_KEY=0"],
    "other_device": ["-DEXPECT_NOTE_KEY=0"],
}
with tempfile.TemporaryDirectory() as directory:
    for name, defines in configurations.items():
        exe = str(Path(directory) / name)
        subprocess.run([
            os.environ.get("CC", "cc"), "-std=c11", "-O1", "-g", "-Wall", "-Wextra", "-Werror",
            "-Wno-unused-function", "-fsanitize=address,undefined", "-Ilib",
            *defines, "test/midi/test_midi.c", "-o", exe,
        ], cwd=root, check=True)
        subprocess.run([exe], cwd=root, check=True, timeout=30)
