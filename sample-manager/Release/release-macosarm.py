#!/usr/bin/env python3
"""Build/sign/notarize main on Apple Silicon using the latest release version, then upload."""
from native_release import main

if __name__ == '__main__':
    raise SystemExit(main('macos-arm64'))
