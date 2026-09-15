#!/usr/bin/env python3
"""Run the actual firmware aggregation/mailbox code with host platform stubs."""
from pathlib import Path
import subprocess
import tempfile
import unittest

root = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory() as directory:
    output = Path(directory) / "metrics"
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=address,undefined", "-g", "-DSEEK_DIAGNOSTICS=1",
                    "-DSAMPLES_PER_BUFFER=441", "-Itest/seek_diagnostics/stubs", "-Ilib",
                    "lib/seek_diagnostics.c", "test/seek_diagnostics/test_metrics.c",
                    "-o", str(output)], cwd=root, check=True)
    subprocess.run([str(output)], check=True)
    trace_object = Path(directory) / "diagnostics.o"
    subprocess.run(["cc", "-std=c11", "-fsanitize=address,undefined", "-g",
                    "-DSEEK_DIAGNOSTICS=1", "-DSAMPLES_PER_BUFFER=441",
                    "-Itest/seek_diagnostics/stubs", "-Ilib", "-c", "lib/seek_diagnostics.c",
                    "-o", str(trace_object)], cwd=root, check=True)
    subprocess.run(["c++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=address,undefined", "-g", "-DSEEK_DIAGNOSTICS=1",
                    "-Itest/seek_diagnostics/stubs", "-Ilib/my_pico_audio/include", "-Ilib",
                    "test/seek_diagnostics/test_audio_trace.cpp", str(trace_object),
                    "-o", str(output)], cwd=root, check=True)
    subprocess.run([str(output)], check=True)
suite = unittest.defaultTestLoader.discover(str(root / "test/seek_diagnostics"))
raise SystemExit(0 if unittest.TextTestRunner(verbosity=2).run(suite).wasSuccessful() else 1)
