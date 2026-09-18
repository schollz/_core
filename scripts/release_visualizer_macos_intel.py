#!/usr/bin/env python3
"""Build on the remote Intel Mac; sign, notarize, and publish on this Mac."""

from pathlib import Path
import sys


RELEASE_DIR = Path(__file__).resolve().parents[1] / "visualizer-juce" / "Release"
sys.path.insert(0, str(RELEASE_DIR))

from release_common import main  # noqa: E402


if __name__ == "__main__":
    raise SystemExit(main("Darwin", "x86_64", "11.6", remote_intel=True))
