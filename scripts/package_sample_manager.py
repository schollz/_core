#!/usr/bin/env python3
"""Build and package Core Sample Manager locally, without publication."""
from pathlib import Path
import runpy

if __name__ == '__main__':
    runpy.run_path(str(Path(__file__).resolve().parents[1] / 'sample-manager' / 'Release' / 'package.py'), run_name='__main__')
