#!/usr/bin/env python3
"""Build main on Linux x86_64 using the latest release version, then package and upload."""
from native_release import main

if __name__ == '__main__':
    raise SystemExit(main('linux-x86_64'))
