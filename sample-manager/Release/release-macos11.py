#!/usr/bin/env python3
"""Build main on Intel using the latest release version; sign, notarize and upload locally."""
from native_release import main

if __name__ == '__main__':
    raise SystemExit(main('macos-x86_64'))
