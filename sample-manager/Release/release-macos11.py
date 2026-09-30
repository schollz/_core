#!/usr/bin/env python3
"""Build the latest release on the Intel Mac; sign, notarize and upload on this Mac."""
from native_release import main

if __name__ == '__main__':
    raise SystemExit(main('macos-x86_64'))
