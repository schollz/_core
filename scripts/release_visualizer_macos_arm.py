#!/usr/bin/env python3
"""Build, sign, notarize, and publish the Apple Silicon visualizer release."""

from pathlib import Path
import sys


RELEASE_DIR = Path(__file__).resolve().parents[1] / "visualizer-juce" / "Release"
sys.path.insert(0, str(RELEASE_DIR))

from release_common import main  # noqa: E402


if __name__ == "__main__":
    raise SystemExit(main("Darwin", "arm64", "12.0"))
