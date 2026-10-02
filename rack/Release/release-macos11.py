#!/usr/bin/env python3
"""Build the Intel Rack plugin remotely and publish from this Mac."""
from rack_release import main

if __name__ == '__main__':
    raise SystemExit(main('mac-x64'))
