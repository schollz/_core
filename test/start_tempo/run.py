"""Test settings IO and the production firmware startup/preset code."""
from pathlib import Path
import os
import argparse
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser()
parser.add_argument("--sanitize", default="undefined", choices=["undefined", "address,undefined"])
args = parser.parse_args()
source = (root / "lib/sdcard_startup.h").read_text()
with tempfile.TemporaryDirectory() as folder:
    out = Path(folder)
    first = source.index("static bool savefile_load_state(bool reopen) {")
    end = source.index("void savefile_do_load", first)
    (out / "preset.h").write_text(source[first:end])
    first = source.index("  sf = SaveFile_malloc();", source.index("void sdcard_startup()"))
    end = source.index("  // initialize sequencers", first)
    (out / "boot.h").write_text(source[first:end])
    for test in ("settings", "startup"):
        exe = out / ("test-" + test)
        subprocess.run([os.environ.get("CC", "clang"), "-std=c11", "-Wall", "-Wextra", "-Werror",
                        "-fsanitize=" + args.sanitize, "-Ilib", "-Itest/sample_cv", f"-I{out}",
                        f"test/start_tempo/test_{test}.c", "-o", str(exe)], cwd=root, check=True)
        subprocess.run([str(exe)], cwd=out, check=True, timeout=30)
subprocess.run(["node", "test/start_tempo/test_webtool.cjs"], cwd=root, check=True)
