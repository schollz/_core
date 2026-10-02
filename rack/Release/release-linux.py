#!/usr/bin/env python3
"""Build and publish the native Linux x86_64 Rack plugin from main."""
from rack_release import main

if __name__ == '__main__':
    raise SystemExit(main('lin-x64'))
