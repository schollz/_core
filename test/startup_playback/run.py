"""Exercise production setting IO and the firmware's boot release boundary."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
main = (root / "main.c").read_text()
startup = (root / "lib/sdcard_startup.h").read_text()
with tempfile.TemporaryDirectory() as folder:
    out = Path(folder)
    first = main.index("void startup_playback_apply(void) {")
    end = main.index("static void ecto_emit_trigger", first)
    (out / "boot_transport.h").write_text(main[first:end])
    first = startup.index("#ifdef INCLUDE_ECTOCORE\n  // Establish silence")
    end = startup.index("  time_of_initialization", first)
    (out / "boot_release.h").write_text(startup[first:end])
    for name in ("settings", "startup"):
        exe = out / ("test-" + name)
        subprocess.run([os.environ.get("CC", "clang"), "-std=c11", "-Wall", "-Wextra", "-Werror",
                        "-fsanitize=undefined", "-Ilib", "-Itest/sample_cv", f"-I{out}",
                        f"test/startup_playback/test_{name}.c", "-o", str(exe)], cwd=root, check=True)
        subprocess.run([str(exe)], cwd=out, check=True, timeout=30)
