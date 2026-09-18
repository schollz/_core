#!/usr/bin/env python3
"""Build and publish the native x86_64 Linux visualizer release."""

from pathlib import Path
import sys


RELEASE_DIR = Path(__file__).resolve().parents[1] / "visualizer-juce" / "Release"
sys.path.insert(0, str(RELEASE_DIR))

from release_common import main  # noqa: E402


if __name__ == "__main__":
    raise SystemExit(main("Linux", "x86_64"))
