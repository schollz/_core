#!/usr/bin/env python3
"""Build over SSH on an Intel Mac; sign, notarize and publish on this Mac."""
from release_common import main

if __name__ == "__main__":
    raise SystemExit(main("Darwin", "x86_64", "11.6", remote_intel=True))
