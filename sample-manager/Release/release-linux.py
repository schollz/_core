#!/usr/bin/env python3
"""Clone, build, package and upload the latest release directly on Linux x86_64."""
from native_release import main

if __name__ == '__main__':
    raise SystemExit(main('linux-x86_64'))
