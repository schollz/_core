#!/usr/bin/env python3
"""Clone the latest release, build/sign/notarize on this Apple Silicon Mac, and upload."""
from native_release import main

if __name__ == '__main__':
    raise SystemExit(main('macos-arm64'))
