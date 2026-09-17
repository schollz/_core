#!/usr/bin/env python3
"""Build, sign and notarize locally on Apple Silicon."""
from release_common import main

if __name__ == "__main__":
    raise SystemExit(main("Darwin", "arm64", "12.0"))
