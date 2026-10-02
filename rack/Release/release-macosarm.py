#!/usr/bin/env python3
"""Build and publish the native Apple Silicon Rack plugin from main."""
from rack_release import main

if __name__ == '__main__':
    raise SystemExit(main('mac-arm64'))
