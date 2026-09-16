#!/usr/bin/env python3
"""Compare optimized DSP output/state against frozen pre-optimization loops."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory() as directory:
    binary = str(Path(directory) / "loops")
    # The existing DSP uses signed wraparound. Make that behavior explicit in
    # the host oracle while checking memory and other undefined operations.
    for optimization in ["-O0", "-O2"]:
        subprocess.run(["cc", "-std=c11", optimization, "-g", "-fwrapv",
                        "-ffp-contract=off", "-fsanitize=address,undefined",
                        "-Ilib", "test/dsp_loops/test_loops.c", "-lm",
                        "-o", binary], cwd=root, check=True)
        subprocess.run([binary], check=True)
