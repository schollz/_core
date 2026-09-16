from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
configurations = {
    "zeptocore": ["-DINCLUDE_ZEPTOCORE=1", "-DEXPECT_NOTE_KEY=1"],
    "zeptocore_midi_default": ["-DINCLUDE_ZEPTOCORE=1", "-DINCLUDE_MIDI=1", "-DEXPECT_NOTE_KEY=1"],
    "zeptocore_midi_off": ["-DINCLUDE_ZEPTOCORE=1", "-DINCLUDE_MIDI=1", "-DZEPTOCORE_VISUALIZER=0", "-DEXPECT_NOTE_KEY=1"],
    "zeptocore_telemetry": ["-DINCLUDE_ZEPTOCORE=1", "-DINCLUDE_MIDI=1", "-DZV_HOST_TEST=1", "-DZEPTOCORE_VISUALIZER=1", "-DEXPECT_NOTE_KEY=1"],
    "disabled": ["-DINCLUDE_ZEPTOCORE=1", "-DMIDI_NOTE_KEY=0", "-DEXPECT_NOTE_KEY=0"],
    "other_device": ["-DEXPECT_NOTE_KEY=0"],
}
with tempfile.TemporaryDirectory() as directory:
    for name, defines in configurations.items():
        exe = str(Path(directory) / name)
        telemetry = ["lib/visualizer_telemetry.c"] if name == "zeptocore_telemetry" else []
        subprocess.run([
            os.environ.get("CC", "cc"), "-std=c11", "-O1", "-g", "-Wall", "-Wextra", "-Werror",
            "-Wno-unused-function", "-fsanitize=address,undefined", "-Ilib",
            *defines, "test/midi/test_midi.c", *telemetry, "-o", exe,
        ], cwd=root, check=True)
        subprocess.run([exe], cwd=root, check=True, timeout=30)
    exe = str(Path(directory) / "visualizer")
    subprocess.run([
        os.environ.get("CC", "cc"), "-std=c11", "-O1", "-g", "-Wall", "-Wextra", "-Werror",
        "-fsanitize=address,undefined", "-pthread", "-Ilib", "-DINCLUDE_ZEPTOCORE=1",
        "-DINCLUDE_MIDI=1", "-DZV_HOST_TEST=1", "-DZEPTOCORE_VISUALIZER=1", "test/midi/test_visualizer.c",
        "lib/visualizer_telemetry.c", "-o", exe,
    ], cwd=root, check=True)
    subprocess.run([exe], cwd=root, check=True, timeout=30)

    # Header-only users must also reject invalid opt-ins (outside CMake).
    for defines in [[], ["-DINCLUDE_ZEPTOCORE=1"], ["-DINCLUDE_MIDI=1"],
                    ["-DINCLUDE_ZEPTOCORE=1", "-DINCLUDE_MIDI=0"]]:
        result = subprocess.run([
            os.environ.get("CC", "cc"), "-x", "c", "-fsyntax-only", "-Ilib",
            "-DZEPTOCORE_VISUALIZER=1", *defines, "-",
        ], input='#include "visualizer_telemetry.h"\n', cwd=root, text=True, capture_output=True)
        assert result.returncode != 0 and "ZEPTOCORE_VISUALIZER requires" in result.stderr
    print("Visualizer default/OFF/ON and invalid-target checks passed")
