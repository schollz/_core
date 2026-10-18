#!/usr/bin/env python3
"""Keep the VCV Library's root manifest identical to rack/plugin.json."""
import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def read_manifest(path):
    if path.is_symlink() or not path.is_file():
        raise ValueError(f'{path}: expected a regular manifest file')
    data = path.read_bytes()
    manifest = json.loads(data)
    if not isinstance(manifest, dict):
        raise ValueError(f'{path}: expected a JSON object')
    return data, manifest


def check(root=ROOT):
    expected, manifest = read_manifest(root / 'rack/plugin.json')
    actual, _ = read_manifest(root / 'plugin.json')
    if actual != expected:
        raise ValueError('plugin.json differs from rack/plugin.json; run '
                         'python3 rack/scripts/manifest.py --sync')
    return manifest


def sync(root=ROOT):
    data, _ = read_manifest(root / 'rack/plugin.json')
    target = root / 'plugin.json'
    if target.is_symlink():
        raise ValueError(f'{target}: expected a regular manifest file')
    target.write_bytes(data)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--check', action='store_true', help='Check without writing (default)')
    mode.add_argument('--sync', action='store_true', help='Copy rack/plugin.json to plugin.json')
    args = parser.parse_args()
    try:
        if args.sync:
            sync()
        check()
    except (ValueError, OSError) as error:
        parser.exit(1, str(error) + '\n')
    print('Root and Rack manifests match')


if __name__ == '__main__':
    main()
