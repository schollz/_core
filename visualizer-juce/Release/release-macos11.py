#!/usr/bin/env python3
"""Build, sign and notarize locally on an Intel Mac (no SSH builder)."""
from release_common import main

if __name__ == "__main__":
    raise SystemExit(main("Darwin", "x86_64", "11.6"))
